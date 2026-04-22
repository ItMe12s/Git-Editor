#pragma once

#include "../model/LevelState.hpp"

#include <optional>

namespace git_editor {

std::optional<LevelState> mergeStates3Way(
    LevelState const& base,
    LevelState const& ours,
    LevelState const& theirs,
    int& conflictCount
);

} // namespace git_editor
