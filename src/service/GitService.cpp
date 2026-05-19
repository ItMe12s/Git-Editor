#include "GitService.hpp"
#include "GdgeImportPlanner.hpp"
#include "MergeService.hpp"
#include "ReconstructionService.hpp"

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../identity/Matcher.hpp"
#include "../model/LevelParser.hpp"
#include "../store/GdgePackage.hpp"
#include "../util/PathUtf8.hpp"
#include "../util/StateHash.hpp"

#include <Geode/loader/Log.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <unordered_map>
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

template <typename T>
Result<T> failResult(std::string msg) {
    Result<T> r;
    r.error = std::move(msg);
    return r;
}

template <typename T>
Result<T> logAndFail(std::string msg) {
    geode::log::error("{}", msg);
    return failResult<T>(std::move(msg));
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

    LevelState headState;
    std::optional<CommitId> parent = m_store.getHead(levelKey);
    if (parent) {
        auto recon = this->reconstruct(*parent);
        if (!recon) return logAndFail<CommitId>("failed to reconstruct HEAD, refusing to commit");
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

    auto id = m_store.insertAndSetHead(levelKey, parent, std::nullopt, message, blob);
    if (!id) return logAndFail<CommitId>("DB insert/head transaction failed");

    this->cachePut(*id, std::move(incoming));

    out.ok     = true;
    out.value  = *id;
    return out;
}

Prepared<LevelState> GitService::prepareCheckout(LevelKey const& levelKey, CommitId target) {
    Prepared<LevelState> out;

    auto head = m_store.getHead(levelKey);
    if (!head) {
        out.result = failResult<LevelState>("no HEAD for this level");
        return out;
    }
    if (*head == target) {
        // No-op checkout: HEAD already there. Skip pending; UI re-applies recon state for parity.
        auto recon = this->reconstruct(target);
        if (!recon) {
            out.result = failResult<LevelState>("reconstruct HEAD failed");
            return out;
        }
        out.result.ok    = true;
        out.result.value = std::move(*recon);
        return out;
    }

    auto headState   = this->reconstruct(*head);
    auto targetState = this->reconstruct(target);
    if (!headState || !targetState) {
        out.result = logAndFail<LevelState>("checkout reconstruct failed");
        return out;
    }

    auto revertDelta = diff(*headState, *targetState);
    auto blob        = dumpDelta(revertDelta);

    auto targetRow = m_store.get(target);
    if (targetRow && targetRow->levelKey != levelKey) {
        out.result = failResult<LevelState>("target commit belongs to a different level");
        return out;
    }
    std::string msg = "Checkout: " + (targetRow ? shortPreview(targetRow->message) : std::to_string(target));

    PendingHeadUpdate pending;
    pending.levelKey  = levelKey;
    pending.parent    = *head;
    pending.reverts   = target;
    pending.message   = std::move(msg);
    pending.deltaBlob = std::move(blob);
    out.pendingHead   = std::move(pending);

    out.result.ok    = true;
    out.result.value = std::move(*targetState);
    return out;
}

Result<CommitId> GitService::finalizeCheckout(
    PendingHeadUpdate const& pending,
    LevelState const&        applied
) {
    auto id = m_store.insertAndSetHead(
        pending.levelKey, pending.parent, pending.reverts, pending.message, pending.deltaBlob
    );
    if (!id) return failResult<CommitId>("DB insert/head transaction failed");
    this->cachePut(*id, applied);
    Result<CommitId> r;
    r.ok    = true;
    r.value = *id;
    return r;
}

Prepared<RevertPayload> GitService::prepareRevert(LevelKey const& levelKey, CommitId target) {
    Prepared<RevertPayload> out;

    auto head = m_store.getHead(levelKey);
    if (!head) {
        out.result = failResult<RevertPayload>("no HEAD for this level");
        return out;
    }

    auto targetRow = m_store.get(target);
    if (!targetRow) {
        out.result = failResult<RevertPayload>("target commit not found");
        return out;
    }
    if (targetRow->levelKey != levelKey) {
        out.result = failResult<RevertPayload>("target commit belongs to a different level");
        return out;
    }
    if (!targetRow->parent) {
        out.result = failResult<RevertPayload>("can't revert the initial commit (it has no parent)");
        return out;
    }

    auto parentState = this->reconstruct(*targetRow->parent);
    auto targetState = this->reconstruct(target);
    auto headState   = this->reconstruct(*head);
    if (!parentState || !targetState || !headState) {
        out.result = failResult<RevertPayload>("reconstruct failed");
        return out;
    }

    // diff(target, parent) not inverse(stored delta): ops use current UUIDs if chain drifted.
    auto undoDelta = diff(*targetState, *parentState);

    RevertPayload value;
    LevelState    headCopy = *headState;
    value.state            = apply(std::move(*headState), undoDelta, &value.conflicts);
    auto persistedDelta    = diff(headCopy, value.state);
    auto blob              = dumpDelta(persistedDelta);

    PendingHeadUpdate pending;
    pending.levelKey  = levelKey;
    pending.parent    = *head;
    pending.reverts   = target;
    pending.message   = "Revert: " + shortPreview(targetRow->message);
    pending.deltaBlob = std::move(blob);
    out.pendingHead   = std::move(pending);

    out.result.ok    = true;
    out.result.value = std::move(value);
    return out;
}

Result<CommitId> GitService::finalizeRevert(
    PendingHeadUpdate const& pending,
    LevelState const&        applied
) {
    // Same shape as finalizeCheckout; kept distinct for call-site readability and future hooks.
    return this->finalizeCheckout(pending, applied);
}

Prepared<LevelState> GitService::prepareSquash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::string const&           message
) {
    Prepared<LevelState> out;

    if (idsOldestFirst.size() < 2) {
        out.result.error = "Squash needs at least 2 commits";
        return out;
    }

    std::vector<CommitRow> rows;
    rows.reserve(idsOldestFirst.size());
    for (auto id : idsOldestFirst) {
        auto row = m_store.get(id);
        if (!row) {
            out.result.error = "Commit " + std::to_string(id) + " not found";
            return out;
        }
        if (row->levelKey != levelKey) {
            out.result.error = "Commit " + std::to_string(id) + " belongs to a different level";
            return out;
        }
        rows.push_back(std::move(*row));
    }

    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (!rows[i].parent || *rows[i].parent != rows[i - 1].id) {
            out.result.error = "Selected commits are not contiguous";
            return out;
        }
    }

    auto const parentOfOldest = rows.front().parent;

    LevelState base;
    if (parentOfOldest) {
        auto recon = this->reconstruct(*parentOfOldest);
        if (!recon) { out.result.error = "reconstruct base failed"; return out; }
        base = std::move(*recon);
    }

    auto target = this->reconstruct(rows.back().id);
    if (!target) { out.result.error = "reconstruct target failed"; return out; }

    auto combined = diff(base, *target);
    auto blob     = dumpDelta(combined);

    PendingSquash pending;
    pending.levelKey       = levelKey;
    pending.idsOldestFirst = idsOldestFirst;
    pending.parentOfOldest = parentOfOldest;
    pending.message        = message;
    pending.deltaBlob      = std::move(blob);
    out.pendingSquash      = std::move(pending);

    out.result.ok    = true;
    out.result.value = std::move(*target);
    return out;
}

Result<CommitId> GitService::finalizeSquash(
    PendingSquash const& pending,
    LevelState const&    applied
) {
    auto newId = m_store.squash(
        pending.levelKey, pending.idsOldestFirst, pending.parentOfOldest,
        pending.message, pending.deltaBlob
    );
    if (!newId) return failResult<CommitId>("DB squash failed");

    this->clearReconstructCache();
    this->cachePut(*newId, applied);

    Result<CommitId> r;
    r.ok    = true;
    r.value = *newId;
    return r;
}

Prepared<LevelState> GitService::prepareImportLevelFrom(
    LevelKey const& dest,
    LevelKey const& src
) {
    Prepared<LevelState> out;
    if (dest == src) {
        out.result.error = "source and destination are the same";
        return out;
    }
    auto const srcHead = m_store.getHead(src);
    if (!srcHead) {
        out.result.error = "source has no HEAD";
        return out;
    }
    auto srcState = this->reconstruct(*srcHead);
    if (!srcState) {
        out.result.error = "reconstruct source HEAD failed";
        return out;
    }

    PendingHistoryReplace pending;
    pending.dest = dest;
    pending.src  = src;
    out.pendingReplace = std::move(pending);

    out.result.ok    = true;
    out.result.value = std::move(*srcState);
    return out;
}

Result<void> GitService::finalizeImportLevelFrom(
    PendingHistoryReplace const& pending,
    LevelState const&            applied
) {
    Result<void> out;
    if (!m_store.replaceLevelHistoryFrom(pending.dest, pending.src)) {
        out.error = "failed to copy level history";
        return out;
    }
    this->clearReconstructCache();
    if (auto const head = m_store.getHead(pending.dest)) {
        // Prime cache so the very next reconstruct doesn't replay the entire copied chain.
        this->cachePut(*head, applied);
    }
    out.ok = true;
    return out;
}

Result<void> GitService::exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath) {
    Result<void> out;
    auto rows = m_store.list(levelKey);
    if (rows.empty()) {
        out.error = "no commits to export";
        return out;
    }
    std::reverse(rows.begin(), rows.end());
    auto head = m_store.getHead(levelKey);
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
    pkg.metadata.sourceLevelKey = levelKey;
    pkg.metadata.headIndex = indexById.at(*head);

    auto root = reconstructRoot(m_store, *this, levelKey);
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

ImportPlan GitService::classifyImports(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    ImportPlan plan;
    plan.noLocalCommits = !m_store.getHead(dest).has_value();
    auto root = reconstructRoot(m_store, *this, dest);
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
    return this->classifyImports(dest, inPaths);
}

Prepared<ImportManyPayload> GitService::prepareImportManyFromGdge(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    Prepared<ImportManyPayload> out;
    if (inPaths.empty()) {
        out.result.error = "no files selected";
        return out;
    }

    auto plan = this->classifyImports(dest, inPaths);
    ImportManyPayload payload;
    payload.skippedCount += static_cast<int>(plan.invalid.size());

    auto headBefore = m_store.getHead(dest);
    LevelState ours;
    LevelState rootBefore;
    if (headBefore) {
        auto root = reconstructRoot(m_store, *this, dest);
        if (!root) {
            out.result.error = "failed to reconstruct current root";
            return out;
        }
        rootBefore = std::move(*root);
        auto recon = this->reconstruct(*headBefore);
        if (!recon) {
            out.result.error = "failed to reconstruct local state";
            return out;
        }
        ours = std::move(*recon);
    }
    // No head: rootBefore stays empty; ours stays empty.

    PendingMergeImport pendingMerge;
    LevelState runningState = ours;
    bool anyMerged = false;
    std::string lastError;

    if (!plan.smart.empty()) {
        LevelState merged = ours;
        int conflicts = 0;
        std::vector<std::string> names;
        names.reserve(plan.smart.size());
        bool ok = true;
        std::string err;
        for (auto const& inPath : plan.smart) {
            auto pkg = readGdgePackage(inPath);
            if (!pkg || pkg->commits.empty() || !pkg->metadata.headIndex) {
                err = "invalid .gdge file";
                ok = false;
                break;
            }
            auto theirs = reconstructPackageHead(*pkg);
            if (!theirs) {
                err = "package history graph invalid";
                ok = false;
                break;
            }
            int stepConflicts = 0;
            auto step = mergeStates3Way(rootBefore, merged, *theirs, stepConflicts);
            if (!step) {
                err = "3-way merge failed";
                ok = false;
                break;
            }
            merged = std::move(*step);
            conflicts += stepConflicts;
            names.push_back(git_editor::pathUtf8(inPath.filename()));
        }
        if (!ok) {
            payload.skippedCount += static_cast<int>(plan.smart.size());
            if (lastError.empty()) lastError = err;
        } else {
            std::string preview;
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (i > 0) preview += ", ";
                preview += names[i];
                if (preview.size() >= 80) break;
            }
            auto message = shortPreview(
                fmt::format("Smart merge: {} imports ({})", plan.smart.size(), preview), 120
            );
            PendingHeadUpdate p;
            p.levelKey  = dest;
            p.parent    = headBefore;
            p.message   = std::move(message);
            p.deltaBlob = dumpDelta(diff(ours, merged));
            pendingMerge.commits.push_back(std::move(p));

            anyMerged = true;
            payload.smartCount = static_cast<int>(plan.smart.size());
            payload.mergedCount += payload.smartCount;
            payload.conflictCount += conflicts;
            runningState = std::move(merged);
            payload.state = runningState;
            if (!headBefore) {
                // No prior head: smart commit is parent=none and becomes the root for subsequent
                // sequential merges to 3-way-merge against.
                rootBefore = runningState;
            }
        }
    }

    for (auto const& path : plan.sequential) {
        auto pkg = readGdgePackage(path);
        if (!pkg || pkg->commits.empty() || !pkg->metadata.headIndex) {
            payload.skippedCount++;
            if (lastError.empty()) {
                lastError = git_editor::pathUtf8(path.filename()) + ": invalid .gdge file";
            }
            continue;
        }
        auto theirs = reconstructPackageHead(*pkg);
        if (!theirs) {
            payload.skippedCount++;
            if (lastError.empty()) {
                lastError = git_editor::pathUtf8(path.filename()) + ": package history graph invalid";
            }
            continue;
        }

        bool const freshRoot = !headBefore && pendingMerge.commits.empty();

        PendingHeadUpdate p;
        p.levelKey = dest;
        if (freshRoot) {
            // First commit on an empty chain: empty parent, blob = diff(empty, theirs). This
            // commit becomes the root, so rootBefore must advance for subsequent 3-way merges.
            p.message    = "Import .gdge: " + git_editor::pathUtf8(path.filename());
            p.deltaBlob  = dumpDelta(diff(LevelState {}, *theirs));
            runningState = *theirs;
            rootBefore   = *theirs;
        } else {
            int conflicts = 0;
            auto merged = mergeStates3Way(rootBefore, runningState, *theirs, conflicts);
            if (!merged) {
                payload.skippedCount++;
                if (lastError.empty()) {
                    lastError = git_editor::pathUtf8(path.filename()) + ": 3-way merge failed";
                }
                continue;
            }
            p.message   = "Merge import: " + git_editor::pathUtf8(path.filename());
            p.deltaBlob = dumpDelta(diff(runningState, *merged));
            if (!pendingMerge.commits.empty()) {
                p.parentPendingIx = pendingMerge.commits.size() - 1;
            } else {
                p.parent = headBefore;
            }
            payload.conflictCount += conflicts;
            runningState = std::move(*merged);
        }
        pendingMerge.commits.push_back(std::move(p));
        anyMerged = true;
        payload.sequentialCount++;
        payload.mergedCount++;
        payload.state = runningState;
    }

    if (!anyMerged) {
        out.result.error = lastError.empty() ? "none of selected files merged" : lastError;
        return out;
    }

    if (!pendingMerge.commits.empty()) {
        // Prime cache for the chain head only; intermediate ids are unlikely to be reconstructed.
        pendingMerge.commits.back().cacheState = payload.state;
    }

    out.result.ok          = true;
    out.result.value       = std::move(payload);
    out.pendingMergeImport = std::move(pendingMerge);
    return out;
}

Result<void> GitService::finalizeImportManyFromGdge(
    PendingMergeImport const& pending,
    LevelState const&         /*applied*/
) {
    Result<void> out;
    if (pending.commits.empty()) {
        out.ok = true;
        return out;
    }
    std::vector<CommitId> minted;
    minted.reserve(pending.commits.size());
    for (std::size_t i = 0; i < pending.commits.size(); ++i) {
        auto const& p = pending.commits[i];
        std::optional<CommitId> parent;
        if (p.parentPendingIx) {
            if (*p.parentPendingIx >= minted.size()) {
                out.error = "pending parent index out of range at entry " + std::to_string(i);
                return out;
            }
            parent = minted[*p.parentPendingIx];
        } else {
            parent = p.parent;
        }
        auto id = m_store.insertAndSetHead(p.levelKey, parent, p.reverts, p.message, p.deltaBlob);
        if (!id) {
            out.error = "insertAndSetHead failed at entry " + std::to_string(i);
            return out;
        }
        minted.push_back(*id);
        if (p.cacheState) this->cachePut(*id, *p.cacheState);
    }
    out.ok = true;
    return out;
}

void GitService::clearReconstructCache() {
    m_cache.clear();
}

Result<LevelState> GitService::checkout(LevelKey const& levelKey, CommitId target) {
    auto prep = this->prepareCheckout(levelKey, target);
    if (!prep.result.ok) return prep.result;
    if (!prep.pendingHead) return prep.result;
    auto fin = this->finalizeCheckout(*prep.pendingHead, prep.result.value);
    if (!fin.ok) return failResult<LevelState>(fin.error);
    return prep.result;
}

Result<RevertPayload> GitService::revert(LevelKey const& levelKey, CommitId target) {
    auto prep = this->prepareRevert(levelKey, target);
    if (!prep.result.ok) return prep.result;
    if (!prep.pendingHead) return prep.result;
    auto fin = this->finalizeRevert(*prep.pendingHead, prep.result.value.state);
    if (!fin.ok) return failResult<RevertPayload>(fin.error);
    return prep.result;
}

Result<LevelState> GitService::squash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::string const&           message
) {
    auto prep = this->prepareSquash(levelKey, idsOldestFirst, message);
    if (!prep.result.ok) return prep.result;
    if (!prep.pendingSquash) return prep.result;
    auto fin = this->finalizeSquash(*prep.pendingSquash, prep.result.value);
    if (!fin.ok) return failResult<LevelState>(fin.error);
    return prep.result;
}

Result<LevelState> GitService::importLevelFrom(LevelKey const& dest, LevelKey const& src) {
    auto prep = this->prepareImportLevelFrom(dest, src);
    if (!prep.result.ok) return prep.result;
    if (!prep.pendingReplace) return prep.result;
    auto fin = this->finalizeImportLevelFrom(*prep.pendingReplace, prep.result.value);
    if (!fin.ok) return failResult<LevelState>(fin.error);
    return prep.result;
}

Result<ImportManyPayload> GitService::importManyFromGdge(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    auto prep = this->prepareImportManyFromGdge(dest, inPaths);
    if (!prep.result.ok) return prep.result;
    if (!prep.pendingMergeImport) return prep.result;
    auto fin = this->finalizeImportManyFromGdge(*prep.pendingMergeImport, prep.result.value.state);
    if (!fin.ok) return failResult<ImportManyPayload>(fin.error);
    return prep.result;
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
