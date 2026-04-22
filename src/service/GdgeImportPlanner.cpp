#include "GdgeImportPlanner.hpp"

#include "../store/GdgePackage.hpp"
#include "../util/StateHash.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace git_editor::gdge_import_planner {

namespace {

std::optional<LevelState> reconstructPackageHead(GdgePackageData const& pkg) {
    if (!pkg.metadata.headIndex) return std::nullopt;
    std::unordered_map<std::int64_t, GdgePackageCommit const*> byIndex;
    byIndex.reserve(pkg.commits.size());
    for (auto const& c : pkg.commits) {
        byIndex[c.commitIndex] = &c;
    }

    std::vector<GdgePackageCommit const*> chain;
    std::unordered_set<std::int64_t> seen;
    std::optional<std::int64_t> cur = pkg.metadata.headIndex;
    while (cur) {
        if (!byIndex.contains(*cur)) return std::nullopt;
        if (seen.contains(*cur)) return std::nullopt;
        seen.insert(*cur);
        auto const* row = byIndex.at(*cur);
        chain.push_back(row);
        cur = row->parentIndex;
    }
    std::reverse(chain.begin(), chain.end());

    LevelState st;
    for (auto const* row : chain) {
        auto d = parseDelta(row->deltaBlob);
        if (!d) return std::nullopt;
        st = apply(std::move(st), *d, nullptr);
    }
    return st;
}

} // namespace

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
