#include "GitService.hpp"
#include "CommitSummaryBuilder.hpp"
#include "GdgeImportMerge.hpp"
#include "GdgeImportPlanner.hpp"

#include "diff/Delta.hpp"
#include "diff/Differ.hpp"
#include "identity/Matcher.hpp"
#include "model/LevelParser.hpp"
#include "store/GdgeExport.hpp"
#include "store/GdgePackage.hpp"
#include "util/format/Shorten.hpp"
#include "util/format/StateHash.hpp"
#include "ui/presentation/DeltaText.hpp"

#include <Geode/loader/Log.hpp>

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
    return std::unexpected(std::move(msg));
}

template <typename T>
Result<T> logAndFail(std::string msg) {
    geode::log::error("{}", msg);
    return failResult<T>(std::move(msg));
}

} // namespace

GitService::GitService(CommitStore& store, std::size_t cacheCapacity)
    : m_store(store), m_cacheCapacity(cacheCapacity) {}

Result<CommitId> GitService::commit(
    LevelKey const& levelKey,
    std::string const& message,
    std::string const& liveLevelStr
) {
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
    if (!id) return logAndFail<CommitId>(id.error());

    this->cachePut(*id, std::move(incoming));

    geode::log::info(
        "commit: recon={:.0f}ms parse={:.0f}ms uuid={:.0f}ms fp={} spatial={} fresh={} "
        "diff={:.0f}ms mod={} adds={} rem={} dump={:.0f}ms blob={}B insert={:.0f}ms total={:.0f}ms",
        reconMs, parseMs, uuidMs,
        uuidStats.fingerprintHits, uuidStats.spatialFallbacks, uuidStats.freshUuids,
        diffMs, delta.modifies.size(), delta.adds.size(), delta.removes.size(),
        dumpMs, blob.size(), insertMs, total.msSince()
    );

    return *id;
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
        out.result = LevelState(*recon);
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

    out.result = LevelState(*targetState);
    return out;
}

Result<CommitId> GitService::finalizeCheckout(
    PendingHeadUpdate const& pending,
    LevelState const&        applied
) {
    auto id = m_store.insertAndSetHead(
        pending.levelKey, pending.parent, pending.reverts, pending.message, pending.deltaBlob
    );
    if (!id) return failResult<CommitId>(id.error());
    this->cachePut(*id, applied);
    return *id;
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

    out.result = std::move(value);
    return out;
}

Prepared<LevelState> GitService::prepareSquash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::string const&           message
) {
    Prepared<LevelState> out;

    if (idsOldestFirst.size() < 2) {
        out.result = failResult<LevelState>("Squash needs at least 2 commits");
        return out;
    }

    std::vector<CommitRow> rows;
    rows.reserve(idsOldestFirst.size());
    for (auto id : idsOldestFirst) {
        auto row = m_store.get(id);
        if (!row) {
            out.result = failResult<LevelState>("Commit " + std::to_string(id) + " not found");
            return out;
        }
        if (row->levelKey != levelKey) {
            out.result = failResult<LevelState>("Commit " + std::to_string(id) + " belongs to a different level");
            return out;
        }
        rows.push_back(std::move(*row));
    }

    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (!rows[i].parent || *rows[i].parent != rows[i - 1].id) {
            out.result = failResult<LevelState>("Selected commits are not contiguous");
            return out;
        }
    }

    auto const parentOfOldest = rows.front().parent;

    LevelState base;
    if (parentOfOldest) {
        auto recon = this->reconstruct(*parentOfOldest);
        if (!recon) { out.result = failResult<LevelState>("reconstruct base failed"); return out; }
        base = LevelState(*recon);
    }

    auto target = this->reconstruct(rows.back().id);
    if (!target) { out.result = failResult<LevelState>("reconstruct target failed"); return out; }

    auto combined = diff(base, *target);
    auto blob     = dumpDelta(combined);

    PendingSquash pending;
    pending.levelKey       = levelKey;
    pending.idsOldestFirst = idsOldestFirst;
    pending.parentOfOldest = parentOfOldest;
    pending.message        = message;
    pending.deltaBlob      = std::move(blob);
    out.pendingSquash      = std::move(pending);

    out.result = LevelState(*target);
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

    return *newId;
}

Prepared<LevelState> GitService::prepareImportLevelFrom(
    LevelKey const& dest,
    LevelKey const& src
) {
    Prepared<LevelState> out;
    if (dest == src) {
        out.result = failResult<LevelState>("source and destination are the same");
        return out;
    }
    auto const srcHead = m_store.getHead(src);
    if (!srcHead) {
        out.result = failResult<LevelState>("source has no HEAD");
        return out;
    }
    auto srcState = this->reconstruct(*srcHead);
    if (!srcState) {
        out.result = failResult<LevelState>("reconstruct source HEAD failed");
        return out;
    }

    PendingHistoryReplace pending;
    pending.dest = dest;
    pending.src  = src;
    out.pendingReplace = std::move(pending);

    out.result = LevelState(*srcState);
    return out;
}

Result<void> GitService::finalizeImportLevelFrom(
    PendingHistoryReplace const& pending,
    LevelState const&            applied
) {
    if (!m_store.replaceLevelHistoryFrom(pending.dest, pending.src)) {
        return std::unexpected("failed to copy level history");
    }
    this->clearReconstructCache();
    if (auto const head = m_store.getHead(pending.dest)) {
        this->cachePut(*head, applied);
    }
    return {};
}

Result<void> GitService::exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath) {
    auto rows = m_store.list(levelKey);
    if (rows.empty()) {
        return std::unexpected("no commits to export");
    }
    auto head = m_store.getHead(levelKey);
    if (!head) {
        return std::unexpected("missing HEAD");
    }

    auto const rootRow = std::find_if(
        rows.begin(), rows.end(), [](CommitRow const& row) { return !row.parent; }
    );
    auto root = rootRow == rows.end() ? LevelStatePtr {} : this->reconstruct(rootRow->id);
    if (!root) {
        return std::unexpected("failed to reconstruct root");
    }

    auto pkgRes = buildGdgePackageFromCommits(levelKey, *head, hashLevelState(*root), rows);
    if (!pkgRes) return std::unexpected(pkgRes.error());

    auto writeRes = writeGdgePackage(outPath, *pkgRes);
    if (!writeRes) {
        return std::unexpected(
            writeRes.error().empty() ? "failed to write .gdge package" : writeRes.error()
        );
    }
    return {};
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
        out.result = failResult<ImportManyPayload>("no files selected");
        return out;
    }

    auto plan = this->classifyImports(dest, inPaths);
    auto headBefore = m_store.getHead(dest);
    LevelState ours;
    LevelState rootBefore;
    if (headBefore) {
        auto root = reconstructRoot(m_store, *this, dest);
        if (!root) {
            out.result = failResult<ImportManyPayload>("failed to reconstruct current root");
            return out;
        }
        rootBefore = LevelState(*root);
        auto recon = this->reconstruct(*headBefore);
        if (!recon) {
            out.result = failResult<ImportManyPayload>("failed to reconstruct local state");
            return out;
        }
        ours = LevelState(*recon);
    }

    return gdge_import_merge::prepareImportManyFromGdge(
        dest, plan, headBefore, std::move(ours), std::move(rootBefore)
    );
}

Result<void> GitService::finalizeImportManyFromGdge(PendingMergeImport const& pending) {
    if (pending.commits.empty()) {
        return {};
    }
    std::vector<CommitId> minted;
    minted.reserve(pending.commits.size());
    for (std::size_t i = 0; i < pending.commits.size(); ++i) {
        auto const& p = pending.commits[i];
        std::optional<CommitId> parent;
        if (p.parentPendingIx) {
            if (*p.parentPendingIx >= minted.size()) {
                return std::unexpected(
                    "pending parent index out of range at entry " + std::to_string(i)
                );
            }
            parent = minted[*p.parentPendingIx];
        } else {
            parent = p.parent;
        }
        auto id = m_store.insertAndSetHead(p.levelKey, parent, p.reverts, p.message, p.deltaBlob);
        if (!id) {
            return std::unexpected(
                "insertAndSetHead failed at entry " + std::to_string(i) + ": " + id.error()
            );
        }
        minted.push_back(*id);
        if (p.cacheState) {
            this->cachePut(*id, std::make_shared<const LevelState>(*p.cacheState));
        }
    }
    return {};
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
        return describeDeltaText(*delta);
    }
    return failResult<std::string>("Could not read this commit's delta.");
}

std::shared_ptr<const LevelState> GitService::reconstruct(CommitId commitId) {
    if (auto hit = this->cacheGet(commitId)) return hit;

    std::vector<CommitRow> chain;
    chain.reserve(32);

    CommitId cur = commitId;
    LevelStatePtr currentState = std::make_shared<const LevelState>();

    while (true) {
        if (auto hit = this->cacheGet(cur)) {
            currentState = std::move(hit);
            break;
        }
        auto row = m_store.get(cur);
        if (!row) {
            geode::log::error("missing commit {} in chain", cur);
            return {};
        }
        chain.push_back(std::move(*row));
        if (!chain.back().parent) break;
        cur = *chain.back().parent;
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto delta = parseDelta(it->deltaBlob);
        if (!delta) {
            geode::log::error("delta for commit {} failed to parse; reconstruct aborted", it->id);
            return {};
        }
        currentState = std::make_shared<const LevelState>(
            apply(LevelState(*currentState), *delta, nullptr)
        );
        this->cachePut(it->id, currentState);
    }

    return currentState;
}

Result<LevelState> GitService::checkout(LevelKey const& levelKey, CommitId target) {
    auto prep = this->prepareCheckout(levelKey, target);
    if (!prep.result) return std::move(prep.result);
    if (!prep.pendingHead) return std::move(prep.result);
    auto fin = this->finalizeCheckout(*prep.pendingHead, *prep.result);
    if (!fin) return failResult<LevelState>(fin.error());
    return std::move(prep.result);
}

Result<RevertPayload> GitService::revert(LevelKey const& levelKey, CommitId target) {
    auto prep = this->prepareRevert(levelKey, target);
    if (!prep.result) return std::move(prep.result);
    if (!prep.pendingHead) return std::move(prep.result);
    auto fin = this->finalizeCheckout(*prep.pendingHead, prep.result->state);
    if (!fin) return failResult<RevertPayload>(fin.error());
    return std::move(prep.result);
}

Result<LevelState> GitService::squash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::string const&           message
) {
    auto prep = this->prepareSquash(levelKey, idsOldestFirst, message);
    if (!prep.result) return std::move(prep.result);
    if (!prep.pendingSquash) return std::move(prep.result);
    auto fin = this->finalizeSquash(*prep.pendingSquash, *prep.result);
    if (!fin) return failResult<LevelState>(fin.error());
    return std::move(prep.result);
}

Result<LevelState> GitService::importLevelFrom(LevelKey const& dest, LevelKey const& src) {
    auto prep = this->prepareImportLevelFrom(dest, src);
    if (!prep.result) return std::move(prep.result);
    if (!prep.pendingReplace) return std::move(prep.result);
    auto fin = this->finalizeImportLevelFrom(*prep.pendingReplace, *prep.result);
    if (!fin) return failResult<LevelState>(fin.error());
    return std::move(prep.result);
}

Result<ImportManyPayload> GitService::importManyFromGdge(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    auto prep = this->prepareImportManyFromGdge(dest, inPaths);
    if (!prep.result) return std::move(prep.result);
    if (!prep.pendingMergeImport) return std::move(prep.result);
    auto fin = this->finalizeImportManyFromGdge(*prep.pendingMergeImport);
    if (!fin) return failResult<ImportManyPayload>(fin.error());
    return std::move(prep.result);
}

ImportPlan GitService::classifyImports(
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& inPaths
) {
    ImportPlan plan;
    plan.noLocalCommits = !m_store.getHead(dest).has_value();
    auto root = reconstructRoot(m_store, *this, dest);
    auto classified = gdge_import_planner::classifyImports(root, inPaths);
    plan.smart = std::move(classified.smart);
    plan.sequential = std::move(classified.sequential);
    plan.invalid = std::move(classified.invalid);
    return plan;
}

void GitService::cachePut(CommitId id, LevelState state) {
    this->cachePut(id, std::make_shared<const LevelState>(std::move(state)));
}

void GitService::cachePut(CommitId id, LevelStatePtr state) {
    auto it = m_cache.find(id);
    if (it != m_cache.end()) {
        it->second = std::move(state);
        return;
    }
    if (m_cache.size() >= m_cacheCapacity) m_cache.clear();
    m_cache.emplace(id, std::move(state));
}

LevelStatePtr GitService::cacheGet(CommitId id) const {
    auto it = m_cache.find(id);
    return it == m_cache.end() ? LevelStatePtr {} : it->second;
}

GitService& sharedGitService() {
    static GitService svc(sharedCommitStore());
    return svc;
}

} // namespace git_editor
