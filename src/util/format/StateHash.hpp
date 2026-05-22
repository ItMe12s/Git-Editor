#pragma once

#include "model/LevelState.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace git_editor {

std::string hashLevelState(LevelState const& state);

} // namespace git_editor
