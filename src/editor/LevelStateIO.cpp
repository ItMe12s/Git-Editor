#include "LevelStateIO.hpp"

#include "../model/LevelParser.hpp"

#include <Geode/loader/Log.hpp>

namespace git_editor {

namespace {

void refreshEditorVisualState(LevelEditorLayer* editor) {
    if (!editor) return;

    editor->loadLevelSettings();
    editor->updateLevelColors();
    editor->syncBGTextures();

    editor->levelSettingsUpdated();
    editor->updateOptions();
    editor->updateEditorMode();
    editor->updateGameObjects();
    editor->updateBlendValues();
    editor->updateArt(0.f);

    if (auto* objects = editor->getAllObjects()) {
        editor->updateObjectColors(objects);
    }
}

} // namespace

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
    if (levelString.empty()) {
        geode::log::warn("applyLevelString rejected empty level string");
        return false;
    }

    if (levelString.find(';') == std::string::npos) {
        geode::log::warn("applyLevelString rejected string without level delimiter");
        return false;
    }

    editor->removeAllObjects();

    if (editor->m_level) {
        editor->m_level->m_levelString = levelString;
        editor->m_level->levelWasAltered();
    }

    gd::string s(levelString.c_str(), levelString.size()); // copy: createObjectsFromSetup mutates
    editor->createObjectsFromSetup(s);
    refreshEditorVisualState(editor);

    auto const applied = captureLevelString(editor);
    if (applied.empty()) {
        geode::log::warn("applyLevelString failed: editor returned empty level string");
        return false;
    }
    return true;
}

bool applyLevelState(LevelEditorLayer* editor, LevelState const& state) {
    return applyLevelString(editor, serializeLevelString(state));
}

} // namespace git_editor
