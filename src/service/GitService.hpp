#pragma once

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"

#include <cstddef>
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

struct ImportLevelOutcome {
    bool        ok    = false;
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

    // Replaces dest history with a deep copy of src, then returns reconstructed HEAD for dest.
    ImportLevelOutcome importLevelFrom(LevelKey const& dest, LevelKey const& src);

    void             clearReconstructCache();

    std::optional<LevelState> reconstruct(CommitId commitId);

private:
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
