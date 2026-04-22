#pragma once

#include "../model/LevelState.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace git_editor {

std::uint64_t fnv1a64(std::string_view text);
std::string   hex64(std::uint64_t value);
std::string   hashLevelState(LevelState const& state);

} // namespace git_editor
