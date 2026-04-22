#pragma once

#include "../model/LevelState.hpp"

#include <Geode/binding/LevelEditorLayer.hpp>
#include <string>

namespace git_editor {

std::string captureLevelString(LevelEditorLayer* editor);

bool applyLevelString(LevelEditorLayer* editor, std::string const& levelString);

bool applyLevelState(LevelEditorLayer* editor, LevelState const& state);

} // namespace git_editor
