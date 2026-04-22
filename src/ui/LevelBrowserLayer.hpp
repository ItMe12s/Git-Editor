#pragma once

#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

namespace git_editor {

class LevelBrowserLayer : public geode::Popup {
public:
    static LevelBrowserLayer* create(
        LevelEditorLayer*  editor,
        EditorPauseLayer*  pauseLayer
    );

protected:
    bool init(LevelEditorLayer* editor, EditorPauseLayer* pauseLayer);

    void rebuildList();

    LevelEditorLayer*  m_editor     = nullptr;
    EditorPauseLayer*  m_pauseLayer = nullptr;
    geode::ScrollLayer* m_scroll     = nullptr;
};

} // namespace git_editor
