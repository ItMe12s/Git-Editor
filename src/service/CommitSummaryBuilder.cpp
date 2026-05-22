#include "CommitSummaryBuilder.hpp"

#include "diff/Delta.hpp"
#include "util/io/BlobCodec.hpp"

namespace git_editor {

std::vector<CommitSummary> buildCommitSummaries(std::vector<CommitSummaryRow> const& rows) {
    std::vector<CommitSummary> out;
    out.reserve(rows.size());
    for (auto const& row : rows) {
        CommitSummary s;
        s.id        = row.id;
        s.message   = row.message;
        s.createdAt = row.createdAt;
        if (!row.deltaBlob.empty()) {
            if (auto json = decompressBlob(row.deltaBlob)) {
                if (auto delta = parseDelta(*json)) {
                    s.headerCount = static_cast<int>(delta->headerChanges.size())
                        + (delta->rawHeaderChange.has_value() ? 1 : 0);
                    s.addCount    = static_cast<int>(delta->adds.size());
                    s.modifyCount = static_cast<int>(delta->modifies.size());
                    s.removeCount = static_cast<int>(delta->removes.size());
                }
            }
        }
        out.push_back(std::move(s));
    }
    return out;
}

} // namespace git_editor
