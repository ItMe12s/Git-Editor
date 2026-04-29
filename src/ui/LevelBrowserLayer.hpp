#pragma once

#include "../store/CommitStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <cstdint>
#include <vector>

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
    void renderList(std::vector<LevelSummary> levels);

    geode::Ref<LevelEditorLayer>  m_editor;
    geode::Ref<EditorPauseLayer>  m_pauseLayer;
    geode::ScrollLayer* m_scroll     = nullptr;
    std::uint64_t       m_loadSerial = 0;
    bool               m_busy       = false;
};

} // namespace git_editor
