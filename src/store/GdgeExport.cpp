#include "GdgeExport.hpp"

#include <unordered_map>
#include <utility>

namespace git_editor {

    Result<GdgePackageData> buildGdgePackageFromCommits(
        LevelKey const& levelKey, CommitId head, std::string const& rootHash,
        std::vector<CommitRow> const& commitsNewestFirst
    ) {
        std::unordered_map<CommitId, std::int64_t> indexById;
        indexById.reserve(commitsNewestFirst.size());
        GdgePackageData pkg;
        pkg.commits.reserve(commitsNewestFirst.size());
        for (std::size_t i = 0; i < commitsNewestFirst.size(); ++i) {
            auto const& row = commitsNewestFirst[commitsNewestFirst.size() - 1 - i];
            indexById[row.id] = static_cast<std::int64_t>(i);
        }
        for (std::size_t i = 0; i < commitsNewestFirst.size(); ++i) {
            auto const& r = commitsNewestFirst[commitsNewestFirst.size() - 1 - i];
            GdgePackageCommit c;
            c.commitIndex = static_cast<std::int64_t>(i);
            if (r.parent) {
                auto it = indexById.find(*r.parent);
                if (it == indexById.end()) {
                    return std::unexpected("parent reference missing during export");
                }
                c.parentIndex = it->second;
            }
            if (r.reverts) {
                auto it = indexById.find(*r.reverts);
                if (it == indexById.end()) {
                    return std::unexpected("reverts reference missing during export");
                }
                c.revertsIndex = it->second;
            }
            c.message = r.message;
            c.createdAt = r.createdAt;
            c.deltaBlob = r.deltaBlob;
            pkg.commits.push_back(std::move(c));
        }
        pkg.metadata.sourceLevelKey = levelKey;
        pkg.metadata.headIndex = indexById.at(head);
        pkg.metadata.rootHash = rootHash;

        return pkg;
    }

} // namespace git_editor
