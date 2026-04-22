#pragma once

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"

#include <cstddef>
#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace git_editor {

struct CommitOutcome {
    bool                    ok = false;
    std::optional<CommitId> id;
    std::string             error;
};

struct CheckoutOutcome {
    bool                    ok = false;
    std::optional<CommitId> revertCommitId;
    LevelState              state;
    std::string             error;
};

struct RevertOutcome {
    bool                    ok = false;
    std::optional<CommitId> revertCommitId;
    LevelState              state;
    std::vector<Conflict>   conflicts;
    std::string             error;
};

struct SquashOutcome {
    bool                    ok = false;
    std::optional<CommitId> newCommitId;
    LevelState              state;
    std::string             error;
};

struct ImportLevelOutcome {
    bool        ok    = false;
    LevelState  state;
    std::string error;
};

struct ExportGdgeOutcome {
    bool        ok = false;
    std::string error;
};

struct ImportManyGdgeOutcome {
    bool        ok = false;
    int         mergedCount = 0;
    int         skippedCount = 0;
    int         conflictCount = 0;
    LevelState  state;
    std::string error;
};

// Linear history, checkout adds forward commit to target state (no rewind HEAD). Persist then setHead then cache.
class GitService {
public:
    explicit GitService(CommitStore& store, std::size_t cacheCapacity = 16);

    CommitOutcome   commit(LevelKey const& levelKey,
                           std::string const& message,
                           std::string const& liveLevelStr);

    CheckoutOutcome checkout(LevelKey const& levelKey, CommitId target);

    RevertOutcome   revert(LevelKey const& levelKey, CommitId target);

    // Collapse a contiguous range of commits into one. idsOldestFirst must form an unbroken
    // parent chain (each id's parent equals the previous id). New commit's parent is the
    // parent of the oldest. HEAD and any later commits are preserved.
    SquashOutcome   squash(LevelKey const&              levelKey,
                           std::vector<CommitId> const& idsOldestFirst,
                           std::string const&           message);

    // Replaces dest history with a deep copy of src, then returns reconstructed HEAD for dest.
    ImportLevelOutcome importLevelFrom(LevelKey const& dest, LevelKey const& src);
    ExportGdgeOutcome exportLevelToGdge(LevelKey const& levelKey, std::filesystem::path const& outPath);
    ImportManyGdgeOutcome importManyFromGdge(LevelKey const& dest,
                                             std::vector<std::filesystem::path> const& inPaths);

    void             clearReconstructCache();

    std::optional<LevelState> reconstruct(CommitId commitId);

private:
    struct MergeSingleResult {
        bool        ok = false;
        int         conflictCount = 0;
        LevelState  state;
        std::string error;
    };
    MergeSingleResult mergeSingleGdge(LevelKey const& canonicalDest,
                                      std::filesystem::path const& inPath);

    void       cachePut(CommitId id, LevelState state);
    std::optional<LevelState> cacheGet(CommitId id);

    CommitStore&                                               m_store;
    std::size_t                                                m_cap;
    std::list<std::pair<CommitId, LevelState>>                 m_lru;
    std::unordered_map<CommitId,
        std::list<std::pair<CommitId, LevelState>>::iterator>  m_index;
};

GitService& sharedGitService();

} // namespace git_editor
