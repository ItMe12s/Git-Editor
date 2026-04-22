#include "LevelStateIO.hpp"

#include "../model/LevelParser.hpp"

#include <Geode/loader/Log.hpp>

namespace git_editor {

std::string captureLevelString(LevelEditorLayer* editor) {
    if (!editor) {
        geode::log::warn("captureLevelString called with null editor");
        return {};
    }
    auto gdStr = editor->getLevelString();
    return std::string(gdStr.c_str(), gdStr.size());
}

bool applyLevelString(LevelEditorLayer* editor, std::string const& levelString) {
    if (!editor) {
        geode::log::warn("applyLevelString called with null editor");
        return false;
    }

    editor->removeAllObjects();

    gd::string s(levelString.c_str(), levelString.size()); // copy: createObjectsFromSetup mutates
    editor->createObjectsFromSetup(s);
    return true;
}

bool applyLevelState(LevelEditorLayer* editor, LevelState const& state) {
    return applyLevelString(editor, serializeLevelString(state));
}

} // namespace git_editor
