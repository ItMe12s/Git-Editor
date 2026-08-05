#pragma once

#include "core/ImportPlan.hpp"
#include "model/LevelState.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace git_editor::gdge_import_planner {

ImportPlan classifyImports(
    std::optional<LevelState> const& rootState,
    std::vector<std::filesystem::path> const& inPaths
);

} // namespace git_editor::gdge_import_planner
