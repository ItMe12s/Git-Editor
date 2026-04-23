#include "GitService.hpp"
#include "GdgeImportPlanner.hpp"
#include "MergeService.hpp"
#include "ReconstructionService.hpp"

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../identity/Matcher.hpp"
#include "../model/LevelParser.hpp"
#include "../store/GdgePackage.hpp"
#include "../util/StateHash.hpp"

#include <Geode/loader/Log.hpp>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace git_editor {

namespace {

std::string shortPreview(std::string s, std::size_t n = 40) {
    if (s.size() <= n) return s;
    return s.substr(0, n - 1) + "...";
}

std::string pathUtf8(std::filesystem::path const& path) {
    auto const u8 = path.u8string();
    return std::string(reinterpret_cast<char const*>(u8.c_str()), u8.size());
}

std::optional<LevelState> reconstructRoot(CommitStore& store, GitService& svc, LevelKey const& levelKey) {
    auto rows = store.list(levelKey);
    if (rows.empty()) return LevelState {};
    auto it = std::find_if(rows.begin(), rows.end(), [](CommitRow const& r) { return !r.parent.has_value(); });
    if (it == rows.end()) return std::nullopt;
    return svc.reconstruct(it->id);
}

template <typename T>
Result<T> failResult(std::string msg) {
    Result<T> r;
    r.error = std::move(msg);
    return r;
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
    : m_store(store), m_cache(cacheCapacity) {}

Result<CommitId> GitService::commit(
    LevelKey const& levelKey,
    std::string const& message,
    std::string const& liveLevelStr
) {
    Result<CommitId> out;
    auto const canonicalKey = m_store.resolveOrCreateCanonicalKey(levelKey);

    LevelState headState;
    std::optional<CommitId> parent = m_store.getHead(canonicalKey);
    if (parent) {
        auto recon = this->reconstruct(*parent);
        if (!recon) {
            auto failed = failResult<CommitId>("failed to reconstruct HEAD, refusing to commit");
            geode::log::error("{}", failed.error);
            return failed;
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
        auto failed = failResult<CommitId>("DB insert failed");
        geode::log::error("{}", failed.error);
        return failed;
    }
    if (!m_store.setHead(canonicalKey, *id)) {
        auto failed = failResult<CommitId>(
            "DB setHead failed (stranded commit " + std::to_string(*id) + ")"
        );
        geode::log::error("{}", failed.error);
        return failed;
    }

    this->cachePut(*id, std::move(incoming));

    out.ok     = true;
    out.value  = *id;
    return out;
}

Result<LevelState> GitService::checkout(LevelKey const& levelKey, CommitId target) {
    Result<LevelState> out;
    auto const canonicalKey = m_store.resolveCanonicalKey(levelKey);

    auto head = m_store.getHead(canonicalKey);
    if (!head) {
        return failResult<LevelState>("no HEAD for this level");
    }
    if (*head == target) {
        auto recon = this->reconstruct(target);
        if (!recon) return failResult<LevelState>("reconstruct HEAD failed");
        out.ok    = true;
        out.value = std::move(*recon);
        return out;
    }

    auto headState   = this->reconstruct(*head);
    auto targetState = this->reconstruct(target);
    if (!headState || !targetState) {
        geode::log::error("checkout reconstruct failed");
        return failResult<LevelState>("reconstruct failed");
    }

    auto revertDelta = diff(*headState, *targetState);
    auto blob        = dumpDelta(revertDelta);

    auto targetRow = m_store.get(target);
    if (targetRow && targetRow->levelKey != canonicalKey) {
        return failResult<LevelState>("target commit belongs to a different level");
    }
    std::string msg = "Checkout: " + (targetRow ? shortPreview(targetRow->message) : std::to_string(target));

    auto id = m_store.insert(canonicalKey, *head, target, msg, blob);
    if (!id) {
        return failResult<LevelState>("DB insert failed");
    }
    if (!m_store.setHead(canonicalKey, *id)) {
        return failResult<LevelState>("DB setHead failed");
    }

    this->cachePut(*id, *targetState);

    out.ok    = true;
    out.value = std::move(*targetState);
    return out;
}

Result<RevertPayload> GitService::revert(LevelKey const& levelKey, CommitId target) {
    Result<RevertPayload> out;
    auto const canonicalKey = m_store.resolveCanonicalKey(levelKey);

    auto head = m_store.getHead(canonicalKey);
    if (!head) {
        return failResult<RevertPayload>("no HEAD for this level");
    }

    auto targetRow = m_store.get(target);
    if (!targetRow) {
        return failResult<RevertPayload>("target commit not found");
    }
    if (targetRow->levelKey != canonicalKey) {
        return failResult<RevertPayload>("target commit belongs to a different level");
    }
    if (!targetRow->parent) {
        return failResult<RevertPayload>("can't revert the initial commit (it has no parent)");
    }

    auto parentState = this->reconstruct(*targetRow->parent);
    auto targetState = this->reconstruct(target);
    auto headState   = this->reconstruct(*head);
    if (!parentState || !targetState || !headState) {
        return failResult<RevertPayload>("reconstruct failed");
    }

    // diff(target, parent) not inverse(stored delta): ops use current UUIDs if chain drifted.
    auto undoDelta = diff(*targetState, *parentState);

    RevertPayload value;
    LevelState    headCopy = *headState;
    value.state            = apply(std::move(*headState), undoDelta, &value.conflicts);
    auto persistedDelta    = diff(headCopy, value.state);
    auto blob              = dumpDelta(persistedDelta);

    std::string msg = "Revert: " + shortPreview(targetRow->message);
    auto id = m_store.insert(canonicalKey, *head, target, msg, blob);
    if (!id) {
        return failResult<RevertPayload>("DB insert failed");
    }
    if (!m_store.setHead(canonicalKey, *id)) {
        return failResult<RevertPayload>("DB setHead failed");
    }

    this->cachePut(*id, value.state);

    out.ok     = true;
    out.value  = std::move(value);
    return out;
}

Result<LevelState> GitService::squash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::string const&           message
) {
    Result<LevelState> out;
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

    out.ok     = true;
    out.value  = std::move(*target);
    return out;
}

Result<LevelState> GitService::importLevelFrom(LevelKey const& dest, LevelKey const& src) {
    Result<LevelState> out;
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
    out.ok     = true;
    out.value  = std::move(*st);
    return out;
}

Result<void> GitService::exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath) {
    Result<void> out;
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

Result<MergeSinglePayload> GitService::mergeSingleGdge(
    LevelKey const& canonicalDest,
    std::filesystem::path const& inPath
) {
    Result<MergeSinglePayload> out;
    auto pkg = readGdgePackage(inPath);
    if (!pkg || pkg->commits.empty() || !pkg->metadata.headIndex) {
        out.error = "invalid .gdge file";
        return out;
    }

    auto theirs = reconstructPackageHead(*pkg);
    if (!theirs) {
        out.error = "package history graph invalid";
        return out;
    }

    auto head = m_store.getHead(canonicalDest);
    if (!head) {
        auto blob = dumpDelta(diff(LevelState {}, *theirs));
        auto id = m_store.insert(
            canonicalDest, std::nullopt, std::nullopt, "Import .gdge: " + pathUtf8(inPath.filename()), blob
        );
        if (!id || !m_store.setHead(canonicalDest, *id)) {
            out.error = "failed to persist imported level";
            return out;
        }
        this->cachePut(*id, *theirs);
        out.ok             = true;
        out.value.state    = std::move(*theirs);
        out.value.conflictCount = 0;
        return out;
    }
    auto root = reconstructRoot(m_store, *this, canonicalDest);
    if (!root) {
        out.error = "failed to reconstruct current root";
        return out;
    }
    auto ours = this->reconstruct(*head);
    if (!ours) {
        out.error = "failed to reconstruct local state";
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
        canonicalDest, *head, std::nullopt, "Merge import: " + pathUtf8(inPath.filename()), blob
    );
    if (!id || !m_store.setHead(canonicalDest, *id)) {
        out.error = "failed to persist merge commit";
        return out;
    }
    this->cachePut(*id, *merged);
    out.ok                  = true;
    out.value.conflictCount = conflicts;
    out.value.state         = std::move(*merged);
    return out;
}

Result<MergeSinglePayload> GitService::smartMergeMany(
    LevelKey const& canonicalDest,
    std::vector<std::filesystem::path> const& paths
) {
    Result<MergeSinglePayload> out;
    if (paths.empty()) {
        out.error = "no smart-mergeable files";
        return out;
    }

    auto head = m_store.getHead(canonicalDest);
    std::optional<LevelState> root = reconstructRoot(m_store, *this, canonicalDest);
    if (!root) {
        out.error = "failed to reconstruct current root";
        return out;
    }
    LevelState base = *root;
    LevelState ours;
    if (head) {
        auto recon = this->reconstruct(*head);
        if (!recon) {
            out.error = "failed to reconstruct local state";
            return out;
        }
        ours = std::move(*recon);
    }
    LevelState merged = ours;
    int totalConflicts = 0;

    std::vector<std::string> names;
    names.reserve(paths.size());
    for (auto const& inPath : paths) {
        auto pkg = readGdgePackage(inPath);
        if (!pkg || pkg->commits.empty() || !pkg->metadata.headIndex) {
            out.error = "invalid .gdge file";
            return out;
        }
        auto theirs = reconstructPackageHead(*pkg);
        if (!theirs) {
            out.error = "package history graph invalid";
            return out;
        }
        int conflicts = 0;
        auto step = mergeStates3Way(base, merged, *theirs, conflicts);
        if (!step) {
            out.error = "3-way merge failed";
            return out;
        }
        merged = std::move(*step);
        totalConflicts += conflicts;
        names.push_back(pathUtf8(inPath.filename()));
    }

    auto persistDelta = diff(ours, merged);
    auto blob = dumpDelta(persistDelta);
    std::string preview;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) preview += ", ";
        preview += names[i];
        if (preview.size() >= 80) break;
    }
    auto message = shortPreview(
        "Smart merge: " + std::to_string(paths.size()) + " imports (" + preview + ")", 120
    );
    auto id = m_store.insert(canonicalDest, head, std::nullopt, message, blob);
    if (!id || !m_store.setHead(canonicalDest, *id)) {
        out.error = "failed to persist merge commit";
        return out;
    }
    this->cachePut(*id, merged);
    out.ok                  = true;
    out.value.conflictCount = totalConflicts;
    out.value.state         = std::move(merged);
    return out;
}

ImportPlan GitService::classifyImports(
    LevelKey const& canonicalDest,
    std::vector<std::filesystem::path> const& inPaths
) {
    ImportPlan plan;
    plan.noLocalCommits = !m_store.getHead(canonicalDest).has_value();
    auto root = reconstructRoot(m_store, *this, canonicalDest);
    auto classified = gdge_import_planner::classifyImports(m_store, root, inPaths);
    plan.localRootHash = std::move(classified.localRootHash);
    plan.smart = std::move(classified.smart);
    plan.sequential = std::move(classified.sequential);
    plan.invalid = std::move(classified.invalid);
    return plan;
}

ImportPlan GitService::planImport(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    if (inPaths.empty()) return {};
    auto const canonicalDest = m_store.resolveOrCreateCanonicalKey(dest);
    return this->classifyImports(canonicalDest, inPaths);
}

Result<ImportManyPayload> GitService::importManyFromGdge(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    Result<ImportManyPayload> out;
    if (inPaths.empty()) {
        out.error = "no files selected";
        return out;
    }

    auto const canonicalDest = m_store.resolveOrCreateCanonicalKey(dest);
    auto plan = this->classifyImports(canonicalDest, inPaths);
    out.value.skippedCount += static_cast<int>(plan.invalid.size());

    bool anyMerged = false;
    std::string lastError;

    if (!plan.smart.empty()) {
        auto smart = this->smartMergeMany(canonicalDest, plan.smart);
        if (!smart.ok) {
            out.value.skippedCount += static_cast<int>(plan.smart.size());
            if (lastError.empty()) lastError = smart.error;
        } else {
            anyMerged = true;
            out.value.smartCount = static_cast<int>(plan.smart.size());
            out.value.mergedCount += out.value.smartCount;
            out.value.conflictCount += smart.value.conflictCount;
            out.value.state = std::move(smart.value.state);
        }
    }

    for (auto const& path : plan.sequential) {
        auto merged = this->mergeSingleGdge(canonicalDest, path);
        if (!merged.ok) {
            out.value.skippedCount++;
            if (lastError.empty()) {
                lastError = pathUtf8(path.filename()) + ": " + merged.error;
            }
            continue;
        }
        anyMerged = true;
        out.value.sequentialCount++;
        out.value.mergedCount++;
        out.value.conflictCount += merged.value.conflictCount;
        out.value.state = std::move(merged.value.state);
    }

    if (!anyMerged) {
        out.error = lastError.empty() ? "none of selected files merged" : lastError;
        return out;
    }
    out.ok = true;
    return out;
}

void GitService::clearReconstructCache() {
    m_cache.clear();
}

std::optional<LevelState> GitService::reconstruct(CommitId commitId) {
    return reconstruction_service::reconstructCommitChain(
        m_store,
        commitId,
        [this](CommitId id) { return this->cacheGet(id); },
        [this](CommitId id, LevelState const& state) { this->cachePut(id, state); }
    );
}

void GitService::cachePut(CommitId id, LevelState state) {
    m_cache.put(id, std::move(state));
}

std::optional<LevelState> GitService::cacheGet(CommitId id) {
    return m_cache.get(id);
}

GitService& sharedGitService() {
    static GitService svc(sharedCommitStore());
    return svc;
}

} // namespace git_editor
