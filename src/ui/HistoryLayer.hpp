#pragma once

#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <string>

namespace git_editor {

// Commit list: checkout (forward commit to that state) and revert (undo that commit's delta).
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
