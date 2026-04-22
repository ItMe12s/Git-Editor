#pragma once

#include "GitService.hpp"

namespace git_editor::gdge_import_planner {

ImportPlan classifyImports(
    CommitStore& store,
    std::optional<LevelState> const& rootState,
    std::vector<std::filesystem::path> const& inPaths
);

} // namespace git_editor::gdge_import_planner
