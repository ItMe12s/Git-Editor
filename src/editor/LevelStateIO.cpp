#include "LevelStateIO.hpp"

#include <Geode/loader/Log.hpp>

namespace git_editor {

std::string captureLevelString(LevelEditorLayer* editor) {
    if (!editor) {
        geode::log::warn("git-editor: captureLevelString called with null editor");
        return {};
    }
    auto gdStr = editor->getLevelString();
    return std::string(gdStr.c_str(), gdStr.size());
}

bool applyLevelString(LevelEditorLayer* editor, std::string const& levelString) {
    if (!editor) {
        geode::log::warn("git-editor: applyLevelString called with null editor");
        return false;
    }

    editor->removeAllObjects();

    // createObjectsFromSetup takes a non-const gd::string& - copy into a local
    // gd::string so the binding can mutate its working buffer freely.
    gd::string s(levelString.c_str(), levelString.size());
    editor->createObjectsFromSetup(s);
    return true;
}

} // namespace git_editor
