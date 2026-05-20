#pragma once

#include "../model/LevelState.hpp"
#include "../store/GdgePackage.hpp"

#include <optional>

namespace git_editor {

std::optional<LevelState> reconstructPackageHead(GdgePackageData const& pkg);

} // namespace git_editor
