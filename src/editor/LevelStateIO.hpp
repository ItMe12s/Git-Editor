#pragma once

#include <Geode/binding/LevelEditorLayer.hpp>
#include <string>

namespace git_editor {

// Capture the level's current serialized form. Returns an empty string on
// failure (null editor, bindings missing).
std::string captureLevelString(LevelEditorLayer* editor);

// Full replace: discard every object currently in the editor and repopulate
// from `levelString`. NOTE: built-in undo/redo history is NOT reset; use the
// editor's own undo if needed.
bool applyLevelString(LevelEditorLayer* editor, std::string const& levelString);

} // namespace git_editor
