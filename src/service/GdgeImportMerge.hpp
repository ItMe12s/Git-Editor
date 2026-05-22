#pragma once

#include "core/ImportPlan.hpp"
#include "model/LevelState.hpp"
#include "store/CommitStore.hpp"
#include "PendingOps.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace git_editor::gdge_import_merge {

Result<LevelState> loadGdgeHead(std::filesystem::path const& path);

Prepared<ImportManyPayload> prepareImportManyFromGdge(
    LevelKey const&         dest,
    ImportPlan const&       plan,
    std::optional<CommitId> headBefore,
    LevelState              ours,
    LevelState              rootBefore
);

} // namespace git_editor::gdge_import_merge
