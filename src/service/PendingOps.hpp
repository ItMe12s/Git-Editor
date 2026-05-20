#pragma once

#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"
#include "../core/Result.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace git_editor {

struct PendingHeadUpdate {
    LevelKey                 levelKey;
    std::optional<CommitId>  parent;
    std::optional<std::size_t> parentPendingIx;
    std::optional<CommitId>  reverts;
    std::string              message;
    // Uncompressed delta JSON (dumpDelta). CommitStore::insertAt compresses on write.
    std::string              deltaBlob;
    std::optional<LevelState> cacheState;
};

struct PendingSquash {
    LevelKey                 levelKey;
    std::vector<CommitId>    idsOldestFirst;
    std::optional<CommitId>  parentOfOldest;
    std::string              message;
    // Uncompressed delta JSON (dumpDelta). CommitStore::insertAt compresses on write.
    std::string              deltaBlob;
};

struct PendingHistoryReplace {
    LevelKey dest;
    LevelKey src;
};

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
