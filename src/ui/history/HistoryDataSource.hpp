#pragma once

#include "../../store/CommitStore.hpp"

#include <vector>

namespace git_editor::history_data_source {

struct HistoryLoadResult {
    LevelKey levelKey;
    std::vector<CommitSummary> commits;
};

HistoryLoadResult loadHistory(LevelKey levelKey, LevelKey activeEditorLevelKey);

} // namespace git_editor::history_data_source
