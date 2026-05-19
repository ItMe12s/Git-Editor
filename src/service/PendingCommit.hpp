#pragma once

#include "../store/CommitStore.hpp"
#include "Result.hpp"

#include <optional>
#include <string>
#include <vector>

namespace git_editor {

// Carries everything needed to persist a single HEAD-advance after the editor has confirmed the
// apply succeeded. Built on the worker; consumed on the worker after the UI signals success.
struct PendingHeadUpdate {
    LevelKey                 levelKey;
    std::optional<CommitId>  parent;
    std::optional<CommitId>  reverts;
    std::string              message;
    std::string              deltaBlob;
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

template <class T>
struct Prepared {
    Result<T>                            result;
    std::optional<PendingHeadUpdate>     pendingHead;
    std::optional<PendingSquash>         pendingSquash;
    std::optional<PendingHistoryReplace> pendingReplace;
    // CommitId of the commit returned by reconstructing the source value for cache priming;
    // when set, finalize will cachePut(state) under the newly-minted commit id.
    std::optional<CommitId>              cacheTargetExisting;
};

} // namespace git_editor
