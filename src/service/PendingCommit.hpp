#pragma once

#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"
#include "Result.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace git_editor {

// Carries everything needed to persist a single HEAD-advance after the editor has confirmed the
// apply succeeded. Built on the worker; consumed on the worker after the UI signals success.
// parentPendingIx: only used inside PendingMergeImport chains, where a pending entry's parent is
// the freshly-minted id of an earlier entry in the same chain. cacheState: optional state to
// cachePut under the newly-minted commit id (chain heads only; intermediate entries leave it unset).
struct PendingHeadUpdate {
    LevelKey                 levelKey;
    std::optional<CommitId>  parent;
    std::optional<std::size_t> parentPendingIx;
    std::optional<CommitId>  reverts;
    std::string              message;
    std::string              deltaBlob;
    std::optional<LevelState> cacheState;
};

// Optional pending squash payload: replaces a range of commits with one new commit.
struct PendingSquash {
    LevelKey                 levelKey;
    std::vector<CommitId>    idsOldestFirst;
    std::optional<CommitId>  parentOfOldest;
    std::string              message;
    std::string              deltaBlob;
};

// Optional pending history replace payload.
struct PendingHistoryReplace {
    LevelKey dest;
    LevelKey src;
};

// Multi-file merge import: N pending head updates replayed in order. Each entry's parent is
// either an existing CommitId (parent) or an earlier entry by index (parentPendingIx). Empty
// commits vector signals "nothing to write" (rare).
struct PendingMergeImport {
    std::vector<PendingHeadUpdate> commits;
};

template <class T>
struct Prepared {
    Result<T>                            result;
    std::optional<PendingHeadUpdate>     pendingHead;
    std::optional<PendingSquash>         pendingSquash;
    std::optional<PendingHistoryReplace> pendingReplace;
    std::optional<PendingMergeImport>    pendingMergeImport;
};

} // namespace git_editor
