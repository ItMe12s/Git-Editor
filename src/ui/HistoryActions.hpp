#pragma once

#include "diff/Differ.hpp"
#include "model/LevelState.hpp"

#include <Geode/binding/LevelEditorLayer.hpp>
#include <vector>

namespace git_editor::history_actions {

bool canApplyEditorResult(LevelEditorLayer* editor);

bool applyStateToEditorOrNotify(
    char const*       noun,
    LevelEditorLayer* editor,
    LevelState const& state,
    bool              hasConflicts
);

void showConflictSummary(std::vector<Conflict> const& conflicts);

} // namespace git_editor::history_actions
