#include "HistoryDataSource.hpp"

namespace git_editor::history_data_source {

HistoryLoadResult loadHistory(LevelKey levelKey, LevelKey activeEditorLevelKey) {
    auto commits = sharedCommitStore().list(levelKey);
    if (commits.empty()) {
        auto const repairedKey = sharedCommitStore().resolveOrCreateCanonicalKey(levelKey);
        if (repairedKey != levelKey) {
            auto repairedCommits = sharedCommitStore().list(repairedKey);
            if (!repairedCommits.empty()) {
                levelKey = repairedKey;
                commits = std::move(repairedCommits);
            }
        }
    }
    if (commits.empty() && !activeEditorLevelKey.empty() && activeEditorLevelKey != levelKey) {
        auto activeCommits = sharedCommitStore().list(activeEditorLevelKey);
        if (!activeCommits.empty()) {
            levelKey = activeEditorLevelKey;
            commits = std::move(activeCommits);
        }
    }

    return {.levelKey = std::move(levelKey), .commits = std::move(commits)};
}

} // namespace git_editor::history_data_source
