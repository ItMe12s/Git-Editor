#pragma once

#include "../model/LevelState.hpp"

#include <Geode/binding/LevelEditorLayer.hpp>
#include <string>

namespace git_editor {

// Capture the level's current serialized form. Returns an empty string on
// failure (null editor, bindings missing).
std::string captureLevelString(LevelEditorLayer* editor);

// Full replace from a raw GD level string. Built-in editor undo/redo is NOT
// reset.
bool applyLevelString(LevelEditorLayer* editor, std::string const& levelString);

// Full replace from a logical LevelState. Internally serializes via
// LevelParser and hands off to applyLevelString.
bool applyLevelState(LevelEditorLayer* editor, LevelState const& state);

} // namespace git_editor
