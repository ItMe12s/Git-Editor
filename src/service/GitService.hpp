#pragma once

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"
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

struct MergeSinglePayload {
    LevelState state;
    int        conflictCount = 0;
};

struct ImportPlan {
    std::vector<std::filesystem::path> smart;
    std::vector<std::filesystem::path> sequential;
    std::vector<std::filesystem::path> invalid;
    std::string localRootHash;
    bool noLocalCommits = false;
};

// Linear history, checkout adds forward commit to target state (no rewind HEAD). Persist then setHead then cache.
class GitService {
public:
    explicit GitService(CommitStore& store, std::size_t cacheCapacity = 64);

    Result<CommitId> commit(LevelKey const& levelKey,
                            std::string const& message,
                            std::string const& liveLevelStr);

    Result<LevelState> checkout(LevelKey const& levelKey, CommitId target);

    Result<RevertPayload> revert(LevelKey const& levelKey, CommitId target);

    // Collapse a contiguous range of commits into one. idsOldestFirst must form an unbroken
    // parent chain (each id's parent equals the previous id). New commit's parent is the
    // parent of the oldest. HEAD and any later commits are preserved.
    Result<LevelState> squash(LevelKey const&              levelKey,
                              std::vector<CommitId> const& idsOldestFirst,
                              std::string const&           message);

    // Replaces dest history with a deep copy of src, then returns reconstructed HEAD for dest.
    Result<LevelState>  importLevelFrom(LevelKey const& dest, LevelKey const& src);
    Result<void>        exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath);
    ImportPlan          planImport(LevelKey const& dest,
                                   std::vector<std::filesystem::path> const& inPaths);
    Result<ImportManyPayload> importManyFromGdge(
        LevelKey const& dest,
        std::vector<std::filesystem::path> const& inPaths
    );

    void             clearReconstructCache();

    std::optional<LevelState> reconstruct(CommitId commitId);

private:
    Result<MergeSinglePayload> mergeSingleGdge(
        LevelKey const& canonicalDest,
        std::filesystem::path const& inPath
    );
    Result<MergeSinglePayload> smartMergeMany(
        LevelKey const& canonicalDest,
        std::vector<std::filesystem::path> const& paths
    );
    ImportPlan classifyImports(LevelKey const& canonicalDest,
                               std::vector<std::filesystem::path> const& inPaths);

    void       cachePut(CommitId id, LevelState state);
    std::optional<LevelState> cacheGet(CommitId id);

    CommitStore& m_store;
    StateCache m_cache;
};

GitService& sharedGitService();

} // namespace git_editor
