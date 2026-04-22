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
    if (levelString.empty()) {
        geode::log::warn("applyLevelString rejected empty level string");
        return false;
    }

    auto const parsedIncoming = parseLevelString(levelString);
    if (parsedIncoming.header.empty() && parsedIncoming.objects.empty()) {
        geode::log::warn("applyLevelString rejected unparseable level string");
        return false;
    }

    auto const normalized = serializeLevelString(parsedIncoming);
    if (normalized.empty()) {
        geode::log::warn("applyLevelString rejected after normalization");
        return false;
    }

    editor->removeAllObjects();

    gd::string s(normalized.c_str(), normalized.size()); // copy: createObjectsFromSetup mutates
    editor->createObjectsFromSetup(s);

    auto const roundTrip = parseLevelString(captureLevelString(editor));
    if (roundTrip.header.empty() && roundTrip.objects.empty()) {
        geode::log::warn("applyLevelString failed to apply setup");
        return false;
    }
    return true;
}

bool applyLevelState(LevelEditorLayer* editor, LevelState const& state) {
    return applyLevelString(editor, serializeLevelString(state));
}

} // namespace git_editor
