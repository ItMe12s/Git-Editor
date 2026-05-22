#include "GdgeExport.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace git_editor {

Result<GdgePackageData> buildGdgePackageFromCommits(
    LevelKey const&               levelKey,
    CommitId                      head,
    std::string const&            rootHash,
    std::vector<CommitRow> const& commitsNewestFirst
) {
    Result<GdgePackageData> out;
    auto rows = commitsNewestFirst;
    std::reverse(rows.begin(), rows.end());

    std::unordered_map<CommitId, std::int64_t> indexById;
    indexById.reserve(rows.size());
    GdgePackageData pkg;
    pkg.commits.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        indexById[rows[i].id] = static_cast<std::int64_t>(i);
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto const& r = rows[i];
        GdgePackageCommit c;
        c.commitIndex = static_cast<std::int64_t>(i);
        if (r.parent) {
            auto it = indexById.find(*r.parent);
            if (it == indexById.end()) {
                out.error = "parent reference missing during export";
                return out;
            }
            c.parentIndex = it->second;
        }
        if (r.reverts) {
            auto it = indexById.find(*r.reverts);
            if (it == indexById.end()) {
                out.error = "reverts reference missing during export";
                return out;
            }
            c.revertsIndex = it->second;
        }
        c.message   = r.message;
        c.createdAt = r.createdAt;
        c.deltaBlob = r.deltaBlob;
        pkg.commits.push_back(std::move(c));
    }
    pkg.metadata.sourceLevelKey = levelKey;
    pkg.metadata.headIndex      = indexById.at(head);
    pkg.metadata.rootHash       = rootHash;

    out.ok    = true;
    out.value = std::move(pkg);
    return out;
}

} // namespace git_editor
