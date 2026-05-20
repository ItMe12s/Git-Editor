#pragma once

#include "../store/CommitStore.hpp"

#include <vector>

namespace git_editor {

std::vector<CommitSummary> buildCommitSummaries(std::vector<CommitSummaryRow> const& rows);

} // namespace git_editor
