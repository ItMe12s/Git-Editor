#pragma once

#include "../../store/CommitStore.hpp"

#include <set>
#include <vector>

namespace git_editor::history_selection_model {

// Works with any row type exposing `.id` (CommitRow, CommitSummary).
template <typename Row>
inline std::vector<CommitId> selectedOldestFirst(
    std::vector<Row> const& commitsDesc,
    std::set<CommitId> const& selected
) {
    std::vector<CommitId> ids;
    ids.reserve(selected.size());
    for (auto it = commitsDesc.rbegin(); it != commitsDesc.rend(); ++it) {
        if (selected.count(it->id)) {
            ids.push_back(it->id);
        }
    }
    return ids;
}

} // namespace git_editor::history_selection_model
