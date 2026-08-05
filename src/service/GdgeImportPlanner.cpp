#include "GdgeImportPlanner.hpp"

#include "PackageReconstruction.hpp"
#include "store/GdgePackage.hpp"
#include "util/format/StateHash.hpp"

namespace git_editor::gdge_import_planner {

    ImportPlan classifyImports(
        std::optional<LevelState> const& rootState, std::vector<std::filesystem::path> const& inPaths
    ) {
        ImportPlan plan;
        if (!rootState) {
            plan.invalid.reserve(inPaths.size());
            for (auto const& path : inPaths) {
                plan.invalid.push_back({path, "local root reconstruct failed"});
            }
            return plan;
        }
        auto const localRootHash = hashLevelState(*rootState);
        for (auto const& path : inPaths) {
            auto pkg = readGdgePackage(path);
            if (!pkg) {
                plan.invalid.push_back({path, pkg.error()});
                continue;
            }
            if (pkg->metadata.rootHash.empty()) {
                plan.invalid.push_back({path, "metadata.rootHash empty"});
                continue;
            }
            if (!reconstructPackageHead(*pkg)) {
                plan.invalid.push_back({path, "package history graph invalid"});
                continue;
            }
            if (pkg->metadata.rootHash == localRootHash) {
                plan.smart.push_back(path);
            }
            else {
                plan.sequential.push_back(path);
            }
        }
        return plan;
    }

} // namespace git_editor::gdge_import_planner
