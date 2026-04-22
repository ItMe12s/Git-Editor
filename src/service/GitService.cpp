#include "GitService.hpp"

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../identity/Matcher.hpp"
#include "../model/LevelParser.hpp"
#include "../store/GdgePackage.hpp"
#include "../util/StateHash.hpp"

#include <Geode/loader/Log.hpp>

#include <algorithm>
#include <set>
#include <unordered_set>
#include <utility>

namespace git_editor {

namespace {

std::string shortPreview(std::string s, std::size_t n = 40) {
    if (s.size() <= n) return s;
    return s.substr(0, n - 1) + "...";
}

std::optional<LevelState> reconstructRoot(CommitStore& store, GitService& svc, LevelKey const& levelKey) {
    auto rows = store.list(levelKey);
    if (rows.empty()) return LevelState {};
    auto it = std::find_if(rows.begin(), rows.end(), [](CommitRow const& r) { return !r.parent.has_value(); });
    if (it == rows.end()) return std::nullopt;
    return svc.reconstruct(it->id);
}

std::optional<LevelState> mergeStates3Way(LevelState const& base,
                                          LevelState const& ours,
                                          LevelState const& theirs,
                                          int& conflictCount) {
    conflictCount = 0;
    LevelState merged = ours;

    auto mergeFieldMap = [&](FieldMap& target, FieldMap const& b, FieldMap const& o, FieldMap const& t) {
        std::set<int> keys;
        for (auto const& [k, _] : b) keys.insert(k);
        for (auto const& [k, _] : o) keys.insert(k);
        for (auto const& [k, _] : t) keys.insert(k);
        for (int k : keys) {
            auto get = [](FieldMap const& m, int key) -> std::string {
                auto it = m.find(key);
                return it == m.end() ? "" : it->second;
            };
            auto vb = get(b, k);
            auto vo = get(o, k);
            auto vt = get(t, k);
            if (vo == vt) {
                if (vo.empty()) target.erase(k);
                else target[k] = vo;
                continue;
            }
            if (vo == vb) {
                if (vt.empty()) target.erase(k);
                else target[k] = vt;
                continue;
            }
            if (vt == vb) {
                if (vo.empty()) target.erase(k);
                else target[k] = vo;
                continue;
            }
            conflictCount++;
        }
    };

    mergeFieldMap(merged.header, base.header, ours.header, theirs.header);
    if (ours.rawHeader == theirs.rawHeader) merged.rawHeader = ours.rawHeader;
    else if (ours.rawHeader == base.rawHeader) merged.rawHeader = theirs.rawHeader;
    else if (theirs.rawHeader == base.rawHeader) merged.rawHeader = ours.rawHeader;
    else conflictCount++;

    std::set<ObjectUuid> ids;
    for (auto const& [id, _] : base.objects) ids.insert(id);
    for (auto const& [id, _] : ours.objects) ids.insert(id);
    for (auto const& [id, _] : theirs.objects) ids.insert(id);
    ObjectUuid nextUuid = 1;
    for (auto const& [id, _] : merged.objects) {
        if (id >= nextUuid) nextUuid = id + 1;
    }

    for (auto id : ids) {
        auto b = base.objects.find(id);
        auto o = ours.objects.find(id);
        auto t = theirs.objects.find(id);
        bool hasB = b != base.objects.end();
        bool hasO = o != ours.objects.end();
        bool hasT = t != theirs.objects.end();

        if (!hasO && !hasT) {
            merged.objects.erase(id);
            continue;
        }
        if (!hasB) {
            if (!hasO && hasT) { merged.objects[id] = t->second; continue; }
            if (hasO && !hasT) { continue; }
            if (o->second.fields == t->second.fields) {
                merged.objects[id] = o->second;
                continue;
            }
            auto importedObj = t->second;
            importedObj.uuid = nextUuid++;
            merged.objects[importedObj.uuid] = std::move(importedObj);
            continue;
        }

        if (hasB && !hasO && hasT) {
            if (t->second.fields == b->second.fields) merged.objects.erase(id);
            else conflictCount++;
            continue;
        }
        if (hasB && hasO && !hasT) {
            if (o->second.fields == b->second.fields) merged.objects.erase(id);
            else conflictCount++;
            continue;
        }
        if (hasO && hasT) {
            auto mergedObj = o->second;
            mergeFieldMap(mergedObj.fields, b->second.fields, o->second.fields, t->second.fields);
            merged.objects[id] = std::move(mergedObj);
        }
    }

    return merged;
}

std::optional<LevelState> reconstructPackageHead(GdgePackageData const& pkg) {
    if (!pkg.metadata.headIndex) return std::nullopt;
    std::unordered_map<std::int64_t, GdgePackageCommit const*> byIndex;
    byIndex.reserve(pkg.commits.size());
    for (auto const& c : pkg.commits) {
        byIndex[c.commitIndex] = &c;
    }

    std::vector<GdgePackageCommit const*> chain;
    std::unordered_set<std::int64_t> seen;
    std::optional<std::int64_t> cur = pkg.metadata.headIndex;
    while (cur) {
        if (!byIndex.contains(*cur)) return std::nullopt;
        if (seen.contains(*cur)) return std::nullopt;
        seen.insert(*cur);
        auto const* row = byIndex.at(*cur);
        chain.push_back(row);
        cur = row->parentIndex;
    }
    std::reverse(chain.begin(), chain.end());

    LevelState st;
    for (auto const* row : chain) {
        auto d = parseDelta(row->deltaBlob);
        if (!d) return std::nullopt;
        st = apply(std::move(st), *d, nullptr);
    }
    return st;
}

} // namespace

GitService::GitService(CommitStore& store, std::size_t cacheCapacity)
    : m_store(store), m_cap(cacheCapacity == 0 ? 1 : cacheCapacity) {}

CommitOutcome GitService::commit(
    LevelKey const& levelKey,
    std::string const& message,
    std::string const& liveLevelStr
) {
    CommitOutcome out;
    auto const canonicalKey = m_store.resolveOrCreateCanonicalKey(levelKey);

    LevelState headState;
    std::optional<CommitId> parent = m_store.getHead(canonicalKey);
    if (parent) {
        auto recon = this->reconstruct(*parent);
        if (!recon) {
            out.error = "failed to reconstruct HEAD, refusing to commit";
            geode::log::error("{}", out.error);
            return out;
        }
        headState = std::move(*recon);
    }

    auto incoming = parseLevelString(liveLevelStr);
    if (parent) assignUuids(headState, incoming);
    else        assignFreshUuids(incoming);

    auto delta = diff(headState, incoming);

    if (delta.adds.empty() && delta.removes.empty()
        && delta.modifies.empty() && delta.headerChanges.empty()) {
        geode::log::info("empty commit '{}'", shortPreview(message));
    }

    auto blob = dumpDelta(delta);

    auto id = m_store.insert(canonicalKey, parent, std::nullopt, message, blob);
    if (!id) {
        out.error = "DB insert failed";
        geode::log::error("{}", out.error);
        return out;
    }
    if (!m_store.setHead(canonicalKey, *id)) {
        out.error = "DB setHead failed (stranded commit " + std::to_string(*id) + ")";
        geode::log::error("{}", out.error);
        return out;
    }

    this->cachePut(*id, std::move(incoming));

    out.ok = true;
    out.id = *id;
    return out;
}

CheckoutOutcome GitService::checkout(LevelKey const& levelKey, CommitId target) {
    CheckoutOutcome out;
    auto const canonicalKey = m_store.resolveCanonicalKey(levelKey);

    auto head = m_store.getHead(canonicalKey);
    if (!head) {
        out.error = "no HEAD for this level";
        return out;
    }
    if (*head == target) {
        auto recon = this->reconstruct(target);
        if (!recon) { out.error = "reconstruct HEAD failed"; return out; }
        out.ok    = true;
        out.state = std::move(*recon);
        return out;
    }

    auto headState   = this->reconstruct(*head);
    auto targetState = this->reconstruct(target);
    if (!headState || !targetState) {
        out.error = "reconstruct failed";
        geode::log::error("checkout reconstruct failed");
        return out;
    }

    auto revertDelta = diff(*headState, *targetState);
    auto blob        = dumpDelta(revertDelta);

    auto targetRow = m_store.get(target);
    if (targetRow && targetRow->levelKey != canonicalKey) {
        out.error = "target commit belongs to a different level";
        return out;
    }
    std::string msg = "Checkout: " + (targetRow ? shortPreview(targetRow->message) : std::to_string(target));

    auto id = m_store.insert(canonicalKey, *head, target, msg, blob);
    if (!id) {
        out.error = "DB insert failed";
        return out;
    }
    if (!m_store.setHead(canonicalKey, *id)) {
        out.error = "DB setHead failed";
        return out;
    }

    this->cachePut(*id, *targetState);

    out.ok             = true;
    out.revertCommitId = *id;
    out.state          = std::move(*targetState);
    return out;
}

RevertOutcome GitService::revert(LevelKey const& levelKey, CommitId target) {
    RevertOutcome out;
    auto const canonicalKey = m_store.resolveCanonicalKey(levelKey);

    auto head = m_store.getHead(canonicalKey);
    if (!head) {
        out.error = "no HEAD for this level";
        return out;
    }

    auto targetRow = m_store.get(target);
    if (!targetRow) {
        out.error = "target commit not found";
        return out;
    }
    if (targetRow->levelKey != canonicalKey) {
        out.error = "target commit belongs to a different level";
        return out;
    }
    if (!targetRow->parent) {
        out.error = "can't revert the initial commit (it has no parent)";
        return out;
    }

    auto parentState = this->reconstruct(*targetRow->parent);
    auto targetState = this->reconstruct(target);
    auto headState   = this->reconstruct(*head);
    if (!parentState || !targetState || !headState) {
        out.error = "reconstruct failed";
        return out;
    }

    // diff(target, parent) not inverse(stored delta): ops use current UUIDs if chain drifted.
    auto undoDelta = diff(*targetState, *parentState);

    LevelState headCopy = *headState;
    auto newState       = apply(std::move(*headState), undoDelta, &out.conflicts);
    auto persistedDelta = diff(headCopy, newState);
    auto blob           = dumpDelta(persistedDelta);

    std::string msg = "Revert: " + shortPreview(targetRow->message);
    auto id = m_store.insert(canonicalKey, *head, target, msg, blob);
    if (!id) {
        out.error = "DB insert failed";
        return out;
    }
    if (!m_store.setHead(canonicalKey, *id)) {
        out.error = "DB setHead failed";
        return out;
    }

    this->cachePut(*id, newState);

    out.ok             = true;
    out.revertCommitId = *id;
    out.state          = std::move(newState);
    return out;
}

SquashOutcome GitService::squash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::string const&           message
) {
    SquashOutcome out;
    auto const canonicalKey = m_store.resolveCanonicalKey(levelKey);

    if (idsOldestFirst.size() < 2) {
        out.error = "Squash needs at least 2 commits";
        return out;
    }

    std::vector<CommitRow> rows;
    rows.reserve(idsOldestFirst.size());
    for (auto id : idsOldestFirst) {
        auto row = m_store.get(id);
        if (!row) {
            out.error = "Commit " + std::to_string(id) + " not found";
            return out;
        }
        if (row->levelKey != canonicalKey) {
            out.error = "Commit " + std::to_string(id) + " belongs to a different level";
            return out;
        }
        rows.push_back(std::move(*row));
    }

    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (!rows[i].parent || *rows[i].parent != rows[i - 1].id) {
            out.error = "Selected commits are not contiguous";
            return out;
        }
    }

    auto const parentOfOldest = rows.front().parent;

    LevelState base;
    if (parentOfOldest) {
        auto recon = this->reconstruct(*parentOfOldest);
        if (!recon) { out.error = "reconstruct base failed"; return out; }
        base = std::move(*recon);
    }

    auto target = this->reconstruct(rows.back().id);
    if (!target) { out.error = "reconstruct target failed"; return out; }

    auto combined = diff(base, *target);
    auto blob     = dumpDelta(combined);

    auto newId = m_store.squash(canonicalKey, idsOldestFirst, parentOfOldest, message, blob);
    if (!newId) {
        out.error = "DB squash failed";
        return out;
    }

    this->clearReconstructCache();

    out.ok          = true;
    out.newCommitId = *newId;
    out.state       = std::move(*target);
    return out;
}

ImportLevelOutcome GitService::importLevelFrom(LevelKey const& dest, LevelKey const& src) {
    ImportLevelOutcome out;
    auto const canonicalDest = m_store.resolveOrCreateCanonicalKey(dest);
    auto const canonicalSrc  = m_store.resolveCanonicalKey(src);
    if (canonicalDest == canonicalSrc) {
        out.error = "source and destination are the same";
        return out;
    }
    if (!m_store.replaceLevelHistoryFrom(canonicalDest, canonicalSrc)) {
        out.error = "failed to copy level history";
        return out;
    }
    this->clearReconstructCache();
    auto const head = m_store.getHead(canonicalDest);
    if (!head) {
        out.error = "no HEAD after import";
        return out;
    }
    auto st = this->reconstruct(*head);
    if (!st) {
        out.error = "reconstruct after import failed";
        return out;
    }
    out.ok    = true;
    out.state = std::move(*st);
    return out;
}

ExportGdgeOutcome GitService::exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath) {
    ExportGdgeOutcome out;
    auto const canonical = m_store.resolveCanonicalKey(levelKey);
    auto rows = m_store.list(canonical);
    if (rows.empty()) {
        out.error = "no commits to export";
        return out;
    }
    std::reverse(rows.begin(), rows.end());
    auto head = m_store.getHead(canonical);
    if (!head) {
        out.error = "missing HEAD";
        return out;
    }

    std::unordered_map<CommitId, std::int64_t> indexById;
    indexById.reserve(rows.size());
    GdgePackageData pkg;
    pkg.commits.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        indexById[rows[i].id] = static_cast<std::int64_t>(i);
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto const& r = rows[i];
        GdgePackageCommit c;
        c.commitIndex = static_cast<std::int64_t>(i);
        if (r.parent) {
            auto it = indexById.find(*r.parent);
            if (it == indexById.end()) {
                out.error = "parent reference missing during export";
                return out;
            }
            c.parentIndex = it->second;
        }
        if (r.reverts) {
            auto it = indexById.find(*r.reverts);
            if (it == indexById.end()) {
                out.error = "reverts reference missing during export";
                return out;
            }
            c.revertsIndex = it->second;
        }
        c.message = r.message;
        c.createdAt = r.createdAt;
        c.deltaBlob = r.deltaBlob;
        pkg.commits.push_back(std::move(c));
    }
    pkg.metadata.sourceLevelKey = canonical;
    pkg.metadata.headIndex = indexById.at(*head);

    auto root = reconstructRoot(m_store, *this, canonical);
    if (!root) {
        out.error = "failed to reconstruct root";
        return out;
    }
    pkg.metadata.rootHash = hashLevelState(*root);

    if (!writeGdgePackage(outPath, pkg)) {
        out.error = "failed to write .gdge package";
        return out;
    }
    out.ok = true;
    return out;
}

GitService::MergeSingleResult GitService::mergeSingleGdge(
    LevelKey const& canonicalDest,
    std::filesystem::path const& inPath
) {
    MergeSingleResult out;
    auto pkg = readGdgePackage(inPath);
    if (!pkg || pkg->commits.empty() || !pkg->metadata.headIndex) {
        out.error = "invalid .gdge file";
        return out;
    }

    auto root = reconstructRoot(m_store, *this, canonicalDest);
    if (!root) {
        out.error = "failed to reconstruct current root";
        return out;
    }
    auto head = m_store.getHead(canonicalDest);
    if (!head) {
        out.error = "no local HEAD";
        return out;
    }
    auto ours = this->reconstruct(*head);
    if (!ours) {
        out.error = "failed to reconstruct local state";
        return out;
    }

    auto theirs = reconstructPackageHead(*pkg);
    if (!theirs) {
        out.error = "package history graph invalid";
        return out;
    }

    int conflicts = 0;
    auto merged = mergeStates3Way(*root, *ours, *theirs, conflicts);
    if (!merged) {
        out.error = "3-way merge failed";
        return out;
    }

    auto persistDelta = diff(*ours, *merged);
    auto blob = dumpDelta(persistDelta);
    auto id = m_store.insert(
        canonicalDest, *head, std::nullopt, "Merge import: " + inPath.filename().string(), blob
    );
    if (!id || !m_store.setHead(canonicalDest, *id)) {
        out.error = "failed to persist merge commit";
        return out;
    }
    this->cachePut(*id, *merged);
    out.ok = true;
    out.conflictCount = conflicts;
    out.state = std::move(*merged);
    return out;
}

ImportManyGdgeOutcome GitService::importManyFromGdge(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    ImportManyGdgeOutcome out;
    if (inPaths.empty()) {
        out.error = "no files selected";
        return out;
    }

    auto const canonicalDest = m_store.resolveOrCreateCanonicalKey(dest);
    bool anyMerged = false;
    std::string lastError;
    for (auto const& path : inPaths) {
        auto merged = this->mergeSingleGdge(canonicalDest, path);
        if (!merged.ok) {
            out.skippedCount++;
            if (lastError.empty()) {
                lastError = path.filename().string() + ": " + merged.error;
            }
            continue;
        }
        anyMerged = true;
        out.mergedCount++;
        out.conflictCount += merged.conflictCount;
        out.state = std::move(merged.state);
    }

    if (!anyMerged) {
        out.error = lastError.empty() ? "none of selected files merged" : lastError;
        return out;
    }
    out.ok = true;
    return out;
}

void GitService::clearReconstructCache() {
    m_lru.clear();
    m_index.clear();
}

std::optional<LevelState> GitService::reconstruct(CommitId commitId) {
    if (auto hit = this->cacheGet(commitId)) {
        return hit;
    }

    // Collect chain root to tip, cache hit on ancestor ends walk early.
    std::vector<CommitRow> chain;
    chain.reserve(32);

    CommitId cur = commitId;
    LevelState baseState;

    while (true) {
        if (auto hit = this->cacheGet(cur)) {
            baseState = std::move(*hit);
            break;
        }
        auto row = m_store.get(cur);
        if (!row) {
            geode::log::error("missing commit {} in chain", cur);
            return std::nullopt;
        }
        chain.push_back(*row);
        if (!row->parent) break;
        cur = *row->parent;
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto delta = parseDelta(it->deltaBlob);
        if (!delta) {
            geode::log::error("delta for commit {} failed to parse; "
                              "reconstruct aborted", it->id);
            return std::nullopt;
        }
        baseState = apply(std::move(baseState), *delta, nullptr);
        this->cachePut(it->id, baseState);
    }

    return baseState;
}

void GitService::cachePut(CommitId id, LevelState state) {
    auto it = m_index.find(id);
    if (it != m_index.end()) {
        it->second->second = std::move(state);
        m_lru.splice(m_lru.begin(), m_lru, it->second);
        return;
    }

    m_lru.emplace_front(id, std::move(state));
    m_index[id] = m_lru.begin();

    while (m_lru.size() > m_cap) {
        auto& victim = m_lru.back();
        m_index.erase(victim.first);
        m_lru.pop_back();
    }
}

std::optional<LevelState> GitService::cacheGet(CommitId id) {
    auto it = m_index.find(id);
    if (it == m_index.end()) return std::nullopt;
    m_lru.splice(m_lru.begin(), m_lru, it->second);
    return it->second->second;
}

GitService& sharedGitService() {
    static GitService svc(sharedCommitStore());
    return svc;
}

} // namespace git_editor
