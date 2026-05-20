#pragma once

#include "LevelState.hpp"

#include <string>
#include <string_view>

namespace git_editor {

LevelState parseLevelString(std::string_view raw);

std::string serializeLevelString(LevelState const& state);

} // namespace git_editor
