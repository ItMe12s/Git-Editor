#include "Matcher.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <deque>
#include <random>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace git_editor {

namespace {

// Any movement below this many GD units between two commits is treated as
// "same object, slightly nudged" by the spatial fallback. Larger than one
// grid cell to tolerate drag-nudge edits, smaller than typical object
// dimensions to stay out of neighbors' territory.
constexpr double kSpatialThreshold = 32.0;

// Process-wide RNG. 64-bit Mersenne twister seeded once from system entropy.
// We only need these to not collide within a level's history, not to be
// cryptographically strong.
std::mt19937_64& rng() {
    static std::mt19937_64 gen{ std::random_device{}() };
    return gen;
}

ObjectUuid freshUuid() {
    // Avoid 0 (we treat it as "unassigned placeholder" in LevelParser).
    ObjectUuid v = 0;
    do {
        v = rng()();
    } while (v == 0);
    return v;
}

double parseDoubleOr(FieldMap const& m, int key, double fallback) {
    auto it = m.find(key);
    if (it == m.end() || it->second.empty()) return fallback;
    double out = fallback;
    auto const& s = it->second;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    (void)ptr;
    return (ec == std::errc()) ? out : fallback;
}

std::string fieldOrEmpty(FieldMap const& m, int key) {
    auto it = m.find(key);
    return (it == m.end()) ? std::string{} : it->second;
}

// Fingerprint buckets aggressively collapse "same-shape" objects. Stacked
// duplicates share a bucket; Matcher then hands them out in insertion order
// so one-to-one pairing is consistent run to run.
struct Fingerprint {
    std::string type;
    int         rx  = 0;
    int         ry  = 0;
    std::string rot;
    std::string groups;

    bool operator==(Fingerprint const& o) const = default;
};

struct FpHash {
    std::size_t operator()(Fingerprint const& f) const noexcept {
        std::size_t h = std::hash<std::string>{}(f.type);
        auto mix = [&](std::size_t v) {
            h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        };
        mix(std::hash<int>{}(f.rx));
        mix(std::hash<int>{}(f.ry));
        mix(std::hash<std::string>{}(f.rot));
        mix(std::hash<std::string>{}(f.groups));
        return h;
    }
};

Fingerprint fingerprintOf(FieldMap const& f) {
    Fingerprint fp;
    fp.type   = fieldOrEmpty(f, key::kType);
    fp.rx     = static_cast<int>(std::lround(parseDoubleOr(f, key::kX, 0.0)));
    fp.ry     = static_cast<int>(std::lround(parseDoubleOr(f, key::kY, 0.0)));
    fp.rot    = fieldOrEmpty(f, key::kRotation);
    fp.groups = fieldOrEmpty(f, key::kGroups);
    return fp;
}

double distanceSquared(FieldMap const& a, FieldMap const& b) {
    double dx = parseDoubleOr(a, key::kX, 0.0) - parseDoubleOr(b, key::kX, 0.0);
    double dy = parseDoubleOr(a, key::kY, 0.0) - parseDoubleOr(b, key::kY, 0.0);
    return dx * dx + dy * dy;
}

} // namespace

void assignFreshUuids(LevelState& state) {
    std::unordered_map<ObjectUuid, Object> rebuilt;
    rebuilt.reserve(state.objects.size());
    for (auto& [_, obj] : state.objects) {
        obj.uuid = freshUuid();
        rebuilt.emplace(obj.uuid, std::move(obj));
    }
    state.objects = std::move(rebuilt);
}

void assignUuids(LevelState const& previous, LevelState& incoming) {
    if (previous.objects.empty()) {
        assignFreshUuids(incoming);
        return;
    }

    // 1) Fingerprint buckets over `previous`. Each bucket is a true FIFO
    //    (std::deque) so repeated fingerprints pair up in their discovery
    //    order, which stays stable because we iterate `previous` in
    //    sorted-uuid order.
    std::unordered_map<Fingerprint, std::deque<ObjectUuid>, FpHash> buckets;
    std::unordered_map<std::string, std::vector<ObjectUuid>> byType;
    std::unordered_set<ObjectUuid> claimed;

    {
        std::vector<ObjectUuid> orderedPrev;
        orderedPrev.reserve(previous.objects.size());
        for (auto const& [u, _] : previous.objects) orderedPrev.push_back(u);
        std::sort(orderedPrev.begin(), orderedPrev.end());

        for (auto u : orderedPrev) {
            auto const& obj = previous.objects.at(u);
            buckets[fingerprintOf(obj.fields)].push_back(u);
            byType[fieldOrEmpty(obj.fields, key::kType)].push_back(u);
        }
    }

    // 2) Walk incoming objects in a deterministic order (their placeholder
    //    uuids from LevelParser, which encode the on-disk order). Note
    //    we're modifying `incoming.objects` after matching so we need to
    //    collect the old placeholder keys first.
    std::vector<ObjectUuid> orderedIncoming;
    orderedIncoming.reserve(incoming.objects.size());
    for (auto const& [u, _] : incoming.objects) orderedIncoming.push_back(u);
    std::sort(orderedIncoming.begin(), orderedIncoming.end());

    std::unordered_map<ObjectUuid, Object> rebuilt;
    rebuilt.reserve(incoming.objects.size());

    for (auto placeholder : orderedIncoming) {
        Object obj = std::move(incoming.objects.at(placeholder));

        ObjectUuid matched = 0;

        // 2a) Exact fingerprint: FIFO, oldest prev uuid wins.
        auto fp   = fingerprintOf(obj.fields);
        auto bkIt = buckets.find(fp);
        if (bkIt != buckets.end()) {
            while (!bkIt->second.empty()) {
                auto cand = bkIt->second.front();
                bkIt->second.pop_front();
                if (!claimed.contains(cand)) { matched = cand; break; }
            }
        }

        // 2b) Spatial nearest-neighbor within type + threshold.
        if (matched == 0) {
            auto typeIt = byType.find(fieldOrEmpty(obj.fields, key::kType));
            if (typeIt != byType.end()) {
                ObjectUuid best    = 0;
                double     bestD2  = kSpatialThreshold * kSpatialThreshold;
                for (auto cand : typeIt->second) {
                    if (claimed.contains(cand)) continue;
                    double d2 = distanceSquared(previous.objects.at(cand).fields, obj.fields);
                    if (d2 < bestD2) {
                        bestD2 = d2;
                        best   = cand;
                    }
                }
                matched = best;
            }
        }

        // 2c) Nothing plausible - mint a new UUID.
        if (matched == 0) matched = freshUuid();
        claimed.insert(matched);

        obj.uuid = matched;
        rebuilt.emplace(matched, std::move(obj));
    }

    incoming.objects = std::move(rebuilt);
}

} // namespace git_editor
