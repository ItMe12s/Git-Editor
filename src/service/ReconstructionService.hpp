#pragma once

#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"

#include <functional>
#include <optional>

namespace git_editor::reconstruction_service {

using CacheGet = std::function<std::optional<LevelState>(CommitId)>;
using CachePut = std::function<void(CommitId, LevelState const&)>;

std::optional<LevelState> reconstructCommitChain(
    CommitStore& store,
    CommitId commitId,
    CacheGet const& cacheGet,
    CachePut const& cachePut
);

} // namespace git_editor::reconstruction_service
