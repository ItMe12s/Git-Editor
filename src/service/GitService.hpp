#pragma once

#include "core/ImportPlan.hpp"
#include "core/Result.hpp"
#include "diff/Delta.hpp"
#include "diff/Differ.hpp"
#include "model/LevelState.hpp"
#include "store/CommitStore.hpp"
#include "PendingOps.hpp"
#include "StateCache.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace git_editor {

class GitService {
public:
    explicit GitService(CommitStore& store, std::size_t cacheCapacity = 64);

    Result<CommitId> commit(
        LevelKey const& levelKey,
        std::string const& message,
        std::string const& liveLevelStr
    );

    Prepared<LevelState> prepareCheckout(LevelKey const& levelKey, CommitId target);
    Result<CommitId> finalizeCheckout(PendingHeadUpdate const& pending, LevelState const& applied);

    Prepared<RevertPayload> prepareRevert(LevelKey const& levelKey, CommitId target);
    Result<CommitId> finalizeRevert(PendingHeadUpdate const& pending, LevelState const& applied);

    Prepared<LevelState> prepareSquash(
        LevelKey const& levelKey,
        std::vector<CommitId> const& idsOldestFirst,
        std::string const& message
    );
    Result<CommitId> finalizeSquash(PendingSquash const& pending, LevelState const& applied);

    Prepared<LevelState> prepareImportLevelFrom(LevelKey const& dest, LevelKey const& src);
    Result<void> finalizeImportLevelFrom(
        PendingHistoryReplace const& pending,
        LevelState const& applied
    );

    Result<void> exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath);
    ImportPlan planImport(
        LevelKey const& dest,
        std::vector<std::filesystem::path> const& inPaths
    );

    Prepared<ImportManyPayload> prepareImportManyFromGdge(
        LevelKey const& dest,
        std::vector<std::filesystem::path> const& inPaths
    );
    Result<void> finalizeImportManyFromGdge(
        PendingMergeImport const& pending,
        LevelState const& applied
    );

    void clearReconstructCache();

    std::vector<CommitSummary> listSummaries(LevelKey const& levelKey);

    bool updateCommitMessage(CommitId id, std::string const& message);

    Result<std::string> describeCommitChanges(CommitId id);

    std::optional<LevelState> reconstruct(CommitId commitId);

    // Test-only helpers. Do not call from UI.
    // They skip the editor-apply gate and can leave DB and editor out of sync.
    Result<LevelState> checkout(LevelKey const& levelKey, CommitId target);
    Result<RevertPayload> revert(LevelKey const& levelKey, CommitId target);
    Result<LevelState> squash(
        LevelKey const& levelKey,
        std::vector<CommitId> const& idsOldestFirst,
        std::string const& message
    );
    Result<LevelState> importLevelFrom(LevelKey const& dest, LevelKey const& src);
    Result<ImportManyPayload> importManyFromGdge(
        LevelKey const& dest,
        std::vector<std::filesystem::path> const& inPaths
    );

private:
    ImportPlan classifyImports(
        LevelKey const& dest,
        std::vector<std::filesystem::path> const& inPaths
    );

    void cachePut(CommitId id, LevelState state);
    std::optional<LevelState> cacheGet(CommitId id);

    CommitStore& m_store;
    StateCache m_cache;
};

GitService& sharedGitService();

} // namespace git_editor
