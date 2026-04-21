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
    std::optional<CommitId> revertCommitId;   // new auto-revert commit id on success
    LevelState              state;            // state to load into the editor
    std::string             error;
};

struct RevertOutcome {
    bool                    ok = false;
    std::optional<CommitId> revertCommitId;
    LevelState              state;
    std::vector<Conflict>   conflicts;        // skipped ops; apply was best-effort
    std::string             error;
};

// Orchestrates the linear commit chain:
//
//   prev HEAD --parent-- new commit
//
// Every operation is transactional at the logic level (we persist the
// commit, advance HEAD, then update the cache - if persistence fails we
// never touch HEAD). There is no branch concept; "checkout" emits a new
// auto-revert commit rather than moving HEAD backwards.
class GitService {
public:
    // cacheCapacity = number of reconstructed LevelStates kept hot. Tune
    // down on memory-constrained platforms; 16 keeps ~15 undo hops instant.
    explicit GitService(CommitStore& store, std::size_t cacheCapacity = 16);

    CommitOutcome   commit(LevelKey const& levelKey,
                           std::string const& message,
                           std::string const& liveLevelStr);

    CheckoutOutcome checkout(LevelKey const& levelKey, CommitId target);

    RevertOutcome   revert(LevelKey const& levelKey, CommitId target);

    // Walk parent chain to rebuild the full state at `commitId`. Results
    // are cached (LRU) for the duration of the process.
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

// Process-wide service bound to `sharedCommitStore()`.
GitService& sharedGitService();

} // namespace git_editor
