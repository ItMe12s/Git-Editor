#pragma once

#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <string>

namespace git_editor {

// Popup: scrollable list of commits for a level. Each row exposes a
// Checkout button that (after a confirm dialog) restores the commit to the
// editor via LevelStateIO::applyLevelString and then dismisses the pause
// layer so the user lands back in the editor.
class HistoryLayer : public geode::Popup {
public:
    static HistoryLayer* create(
        std::string levelKey,
        LevelEditorLayer* editor,
        EditorPauseLayer* pauseLayer
    );

protected:
    bool init(std::string levelKey, LevelEditorLayer* editor, EditorPauseLayer* pauseLayer);

    void rebuildList();

    std::string         m_levelKey;
    LevelEditorLayer*   m_editor     = nullptr;
    EditorPauseLayer*   m_pauseLayer = nullptr;
    geode::ScrollLayer* m_scroll     = nullptr;
};

} // namespace git_editor
