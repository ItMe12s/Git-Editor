#pragma once

#include "../model/LevelState.hpp"

namespace git_editor {

void assignUuids(LevelState const& previous, LevelState& incoming);

void assignFreshUuids(LevelState& state);

} // namespace git_editor
