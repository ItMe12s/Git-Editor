#pragma once

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"
#include "PendingCommit.hpp"
#include "Result.hpp"
#include "StateCache.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace git_editor {

struct RevertPayload {
    LevelState            state;
    std::vector<Conflict> conflicts;
};

struct ImportManyPayload {
    LevelState state;
    int        mergedCount     = 0;
    int        skippedCount    = 0;
    int        conflictCount   = 0;
    int        smartCount      = 0;
    int        sequentialCount = 0;
};

struct InvalidImport {
    std::filesystem::path path;
    std::string           reason;
};

struct ImportPlan {
    std::vector<std::filesystem::path> smart;
    std::vector<std::filesystem::path> sequential;
    std::vector<InvalidImport>         invalid;
    std::string localRootHash;
    bool noLocalCommits = false;
};

// Linear history, checkout adds forward commit to target state (no rewind HEAD).
//
// Two-phase commit: prepare* runs on the worker and computes the next LevelState plus a Pending*
// payload but writes nothing. Once the UI confirms the editor accepted the new state, the caller
// invokes the matching finalize* (also worker) to persist. Apply-failure paths drop pending and
// the DB stays untouched, eliminating the legacy "DB committed but editor unchanged" split-brain.
class GitService {
public:
    explicit GitService(CommitStore& store, std::size_t cacheCapacity = 64);

    Result<CommitId> commit(LevelKey const& levelKey,
                            std::string const& message,
                            std::string const& liveLevelStr);

    // Two-phase: prepare reconstructs target without touching DB; finalize writes the head update.
    Prepared<LevelState> prepareCheckout(LevelKey const& levelKey, CommitId target);
    Result<CommitId>     finalizeCheckout(PendingHeadUpdate const& pending, LevelState const& applied);

    Prepared<RevertPayload> prepareRevert(LevelKey const& levelKey, CommitId target);
    Result<CommitId>        finalizeRevert(PendingHeadUpdate const& pending, LevelState const& applied);

    // Collapse a contiguous range of commits into one. idsOldestFirst must form an unbroken
    // parent chain (each id's parent equals the previous id). New commit's parent is the
    // parent of the oldest. HEAD and any later commits are preserved.
    Prepared<LevelState> prepareSquash(LevelKey const&              levelKey,
                                       std::vector<CommitId> const& idsOldestFirst,
                                       std::string const&           message);
    Result<CommitId>     finalizeSquash(PendingSquash const& pending, LevelState const& applied);

    // Replaces dest history with a deep copy of src, then returns reconstructed HEAD for dest.
    Prepared<LevelState> prepareImportLevelFrom(LevelKey const& dest, LevelKey const& src);
    Result<void>         finalizeImportLevelFrom(PendingHistoryReplace const& pending,
                                                  LevelState const&            applied);

    Result<void>        exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath);
    ImportPlan          planImport(LevelKey const& dest,
                                   std::vector<std::filesystem::path> const& inPaths);

    // Two-phase multi-merge: prepare computes merged state + ordered pending head updates without
    // touching the DB; finalize replays the updates after the UI confirms apply succeeded.
    Prepared<ImportManyPayload> prepareImportManyFromGdge(
        LevelKey const& dest,
        std::vector<std::filesystem::path> const& inPaths
    );
    Result<void> finalizeImportManyFromGdge(
        PendingMergeImport const& pending,
        LevelState const&         applied
    );

    void             clearReconstructCache();

    std::optional<LevelState> reconstruct(CommitId commitId);

    // Test/headless conveniences: prepare + finalize in one call. NOT for UI callers — these
    // skip the editor-apply gate and so reintroduce split-brain risk if used from interactive
    // flows. Used by AutomatedTestHarness where no LevelEditorLayer exists.
    Result<LevelState>    checkout(LevelKey const& levelKey, CommitId target);
    Result<RevertPayload> revert(LevelKey const& levelKey, CommitId target);
    Result<LevelState>    squash(LevelKey const& levelKey,
                                  std::vector<CommitId> const& idsOldestFirst,
                                  std::string const& message);
    Result<LevelState>    importLevelFrom(LevelKey const& dest, LevelKey const& src);
    Result<ImportManyPayload> importManyFromGdge(
        LevelKey const& dest,
        std::vector<std::filesystem::path> const& inPaths);

private:
    ImportPlan classifyImports(LevelKey const& dest,
                               std::vector<std::filesystem::path> const& inPaths);

    void       cachePut(CommitId id, LevelState state);
    std::optional<LevelState> cacheGet(CommitId id);

    CommitStore& m_store;
    StateCache m_cache;
};

GitService& sharedGitService();

} // namespace git_editor
