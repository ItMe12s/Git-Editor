#include "PackageReconstruction.hpp"

#include "diff/Delta.hpp"
#include "diff/Differ.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace git_editor {

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

} // namespace git_editor
