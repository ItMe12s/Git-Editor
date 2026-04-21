#include "LevelParser.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace git_editor {

namespace {

// Split `s` on `delim` without allocating intermediate strings. Empty
// segments are preserved so "a,,b" yields ["a", "", "b"].
std::vector<std::string_view> splitView(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.emplace_back(s.data() + start, i - start);
            start = i + 1;
        }
    }
    return out;
}

// Parse an integer key. Returns true on success, leaves `out` untouched on
// failure (empty strings, non-digits, etc.).
bool parseKey(std::string_view s, int& out) {
    if (s.empty()) return false;
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size()) return false;
    out = value;
    return true;
}

// Reads the "k,v,k,v,..." chunk into a FieldMap. Malformed segments (odd
// number of tokens, non-integer keys) are skipped.
FieldMap readKvChunk(std::string_view chunk) {
    FieldMap out;
    if (chunk.empty()) return out;

    auto tokens = splitView(chunk, ',');
    // If trailing comma produced an empty tail, drop it.
    if (!tokens.empty() && tokens.back().empty()) tokens.pop_back();

    for (std::size_t i = 0; i + 1 < tokens.size(); i += 2) {
        int k = 0;
        if (!parseKey(tokens[i], k)) continue;
        out.emplace(k, std::string(tokens[i + 1]));
    }
    return out;
}

} // namespace

LevelState parseLevelString(std::string_view raw) {
    LevelState state;

    auto chunks = splitView(raw, ';');
    if (chunks.empty()) return state;

    // Chunk 0 = header. Everything after = objects. GD emits a trailing ';'
    // which gives us an empty final chunk we skip.
    state.header = readKvChunk(chunks.front());

    for (std::size_t i = 1; i < chunks.size(); ++i) {
        if (chunks[i].empty()) continue;
        Object obj;
        obj.fields = readKvChunk(chunks[i]);
        if (obj.fields.empty()) continue;
        // Placeholder uuid == chunk index (1..N). These are never persisted
        // - Matcher::assignUuids rewrites them using fingerprint + spatial
        // matching before the LevelState reaches CommitStore. The index
        // form gives us a stable insertion order for deterministic
        // matching later in Matcher.
        obj.uuid = static_cast<ObjectUuid>(i);
        state.objects.emplace(obj.uuid, std::move(obj));
    }

    return state;
}

std::string serializeLevelString(LevelState const& state) {
    std::string out;
    // Rough guess: header is small, each object averages ~80 chars. Reserve
    // to cut reallocation churn in huge levels.
    out.reserve(256 + state.objects.size() * 96);

    auto appendFields = [&](FieldMap const& m) {
        bool first = true;
        for (auto const& [k, v] : m) {
            if (!first) out.push_back(',');
            first = false;
            out.append(std::to_string(k));
            out.push_back(',');
            out.append(v);
        }
    };

    appendFields(state.header);
    out.push_back(';');

    // Deterministic object order by UUID so the serialized output is stable
    // for hashing / diffing even though the std::unordered_map above isn't.
    std::vector<ObjectUuid> ids;
    ids.reserve(state.objects.size());
    for (auto const& [id, _] : state.objects) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    for (auto id : ids) {
        auto const& obj = state.objects.at(id);
        appendFields(obj.fields);
        out.push_back(';');
    }

    return out;
}

} // namespace git_editor
