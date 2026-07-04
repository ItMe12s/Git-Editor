#include "GitService.hpp"
#include "CommitSummaryBuilder.hpp"
#include "GdgeImportMerge.hpp"
#include "GdgeImportPlanner.hpp"
#include "ReconstructionService.hpp"

#include "diff/Delta.hpp"
#include "diff/Differ.hpp"
#include "identity/Matcher.hpp"
#include "model/LevelParser.hpp"
#include "store/GdgeExport.hpp"
#include "store/GdgePackage.hpp"
#include "util/format/Shorten.hpp"
#include "util/format/StateHash.hpp"
#include "ui/presentation/DeltaText.hpp"
#include "util/io/PathUtf8.hpp"

#include <Geode/loader/Log.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <utility>

namespace git_editor {

namespace {

std::optional<LevelState> reconstructRoot(CommitStore& store, GitService& svc, LevelKey const& levelKey) {
    auto rows = store.list(levelKey);
    if (rows.empty()) return LevelState {};
    auto it = std::find_if(rows.begin(), rows.end(), [](CommitRow const& r) { return !r.parent.has_value(); });
    if (it == rows.end()) return std::nullopt;
    if (auto recon = svc.reconstruct(it->id)) return LevelState(*recon);
    return std::nullopt;
}

struct PhaseTimer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double msSince() const {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0
        ).count();
    }
    double lapMs() {
        auto const now = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(now - t0).count();
        t0 = now;
        return ms;
    }
};

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
    PhaseTimer total;
    PhaseTimer phase;

    LevelStatePtr headState;
    std::optional<CommitId> parent = m_store.getHead(levelKey);
    if (parent) {
        headState = this->reconstruct(*parent);
        if (!headState) return logAndFail<CommitId>("failed to reconstruct HEAD, refusing to commit");
    }
    double const reconMs = phase.lapMs();

    auto incoming = parseLevelString(liveLevelStr);
    double const parseMs = phase.lapMs();

    AssignUuidStats uuidStats;
    if (parent) assignUuids(*headState, incoming, &uuidStats);
    else        assignFreshUuids(incoming);
    double const uuidMs = phase.lapMs();

    LevelState const emptyHead;
    LevelState const& headRef = headState ? *headState : emptyHead;
    auto delta = diff(headRef, incoming);
    double const diffMs = phase.lapMs();

    if (delta.adds.empty() && delta.removes.empty()
        && delta.modifies.empty() && delta.headerChanges.empty()) {
        geode::log::info("empty commit '{}'", shorten(message, 40));
    }

    auto blob = dumpDelta(delta);
    double const dumpMs = phase.lapMs();

    auto id = m_store.insertAndSetHead(levelKey, parent, std::nullopt, message, blob);
    double const insertMs = phase.lapMs();
    if (!id.ok) return logAndFail<CommitId>(id.error);

    this->cachePut(id.value, std::move(incoming));

    geode::log::info(
        "commit: recon={:.0f}ms parse={:.0f}ms uuid={:.0f}ms fp={} spatial={} fresh={} "
        "diff={:.0f}ms mod={} adds={} rem={} dump={:.0f}ms blob={}B insert={:.0f}ms total={:.0f}ms",
        reconMs, parseMs, uuidMs,
        uuidStats.fingerprintHits, uuidStats.spatialFallbacks, uuidStats.freshUuids,
        diffMs, delta.modifies.size(), delta.adds.size(), delta.removes.size(),
        dumpMs, blob.size(), insertMs, total.msSince()
    );

    out.ok     = true;
    out.value  = id.value;
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
        auto recon = this->reconstruct(target);
        if (!recon) {
            out.result = failResult<LevelState>("reconstruct HEAD failed");
            return out;
        }
        out.result.ok    = true;
        out.result.value = LevelState(*recon);
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
    std::string msg = "Checkout: " + (targetRow ? shorten(targetRow->message, 40) : std::to_string(target));

    PendingHeadUpdate pending;
    pending.levelKey  = levelKey;
    pending.parent    = *head;
    pending.reverts   = target;
    pending.message   = std::move(msg);
    pending.deltaBlob = std::move(blob);
    out.pendingHead   = std::move(pending);

    out.result.ok    = true;
    out.result.value = LevelState(*targetState);
    return out;
}

Result<CommitId> GitService::finalizeCheckout(
    PendingHeadUpdate const& pending,
    LevelState const&        applied
) {
    auto id = m_store.insertAndSetHead(
        pending.levelKey, pending.parent, pending.reverts, pending.message, pending.deltaBlob
    );
    if (!id.ok) return failResult<CommitId>(id.error);
    this->cachePut(id.value, applied);
    Result<CommitId> r;
    r.ok    = true;
    r.value = id.value;
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

    // Use diff(target, parent), not inverse stored delta, when UUIDs drifted.
    auto undoDelta = diff(*targetState, *parentState);

    RevertPayload value;
    LevelState    headCopy = *headState;
    LevelState    headMut  = *headState;
    value.state            = apply(std::move(headMut), undoDelta, &value.conflicts);
    auto persistedDelta    = diff(headCopy, value.state);
    auto blob              = dumpDelta(persistedDelta);

    PendingHeadUpdate pending;
    pending.levelKey  = levelKey;
    pending.parent    = *head;
    pending.reverts   = target;
    pending.message   = "Revert: " + shorten(targetRow->message, 40);
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
        base = LevelState(*recon);
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
    out.result.value = LevelState(*target);
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
    out.result.value = LevelState(*srcState);
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
    auto head = m_store.getHead(levelKey);
    if (!head) {
        out.error = "missing HEAD";
        return out;
    }

    auto root = reconstructRoot(m_store, *this, levelKey);
    if (!root) {
        out.error = "failed to reconstruct root";
        return out;
    }

    auto pkgRes = buildGdgePackageFromCommits(levelKey, *head, hashLevelState(*root), rows);
    if (!pkgRes.ok) {
        out.error = pkgRes.error;
        return out;
    }

    auto writeRes = writeGdgePackage(outPath, pkgRes.value);
    if (!writeRes.ok) {
        out.error = writeRes.error.empty() ? "failed to write .gdge package" : writeRes.error;
        return out;
    }
    out.ok = true;
    return out;
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
    auto headBefore = m_store.getHead(dest);
    LevelState ours;
    LevelState rootBefore;
    if (headBefore) {
        auto root = reconstructRoot(m_store, *this, dest);
        if (!root) {
            out.result.error = "failed to reconstruct current root";
            return out;
        }
        rootBefore = LevelState(*root);
        auto recon = this->reconstruct(*headBefore);
        if (!recon) {
            out.result.error = "failed to reconstruct local state";
            return out;
        }
        ours = LevelState(*recon);
    }

    return gdge_import_merge::prepareImportManyFromGdge(
        dest, plan, headBefore, std::move(ours), std::move(rootBefore)
    );
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
        if (!id.ok) {
            out.error = "insertAndSetHead failed at entry " + std::to_string(i) + ": " + id.error;
            return out;
        }
        minted.push_back(id.value);
        if (p.cacheState) {
            this->cachePut(id.value, std::make_shared<const LevelState>(*p.cacheState));
        }
    }
    out.ok = true;
    return out;
}

void GitService::clearReconstructCache() {
    m_cache.clear();
}

std::vector<CommitSummary> GitService::listSummaries(LevelKey const& levelKey) {
    return buildCommitSummaries(m_store.listSummaryRows(levelKey));
}

bool GitService::updateCommitMessage(CommitId id, std::string const& message) {
    return m_store.updateMessage(id, message);
}

Result<std::string> GitService::describeCommitChanges(CommitId id) {
    auto row = m_store.get(id);
    if (!row) return failResult<std::string>("Commit not found.");
    if (auto delta = parseDelta(row->deltaBlob)) {
        Result<std::string> out;
        out.ok    = true;
        out.value = describeDeltaText(*delta);
        return out;
    }
    return failResult<std::string>("Could not read this commit's delta.");
}

std::shared_ptr<const LevelState> GitService::reconstruct(CommitId commitId) {
    return reconstruction_service::reconstructCommitChain(
        m_store,
        commitId,
        [this](CommitId id) { return this->cacheGet(id); },
        [this](CommitId id, LevelStatePtr state) { this->cachePut(id, std::move(state)); }
    );
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

void GitService::cachePut(CommitId id, LevelState state) {
    m_cache.put(id, std::make_shared<const LevelState>(std::move(state)));
}

void GitService::cachePut(CommitId id, LevelStatePtr state) {
    m_cache.put(id, std::move(state));
}

LevelStatePtr GitService::cacheGet(CommitId id) {
    return m_cache.get(id);
}

GitService& sharedGitService() {
    static GitService svc(sharedCommitStore());
    return svc;
}

} // namespace git_editor
