#include "GdgeImportPlanner.hpp"

#include "../store/GdgePackage.hpp"
#include "../util/StateHash.hpp"

namespace git_editor::gdge_import_planner {

ImportPlan classifyImports(
    CommitStore& store,
    std::optional<LevelState> const& rootState,
    std::vector<std::filesystem::path> const& inPaths
) {
    ImportPlan plan;
    if (!rootState) {
        plan.invalid = inPaths;
        return plan;
    }
    plan.localRootHash = hashLevelState(*rootState);
    for (auto const& path : inPaths) {
        auto pkg = readGdgePackage(path);
        if (!pkg || pkg->metadata.rootHash.empty()) {
            plan.invalid.push_back(path);
            continue;
        }
        if (!reconstructPackageHead(*pkg)) {
            plan.invalid.push_back(path);
            continue;
        }
        if (pkg->metadata.rootHash == plan.localRootHash) {
            plan.smart.push_back(path);
        } else {
            plan.sequential.push_back(path);
        }
    }
    return plan;
}

} // namespace git_editor::gdge_import_planner
