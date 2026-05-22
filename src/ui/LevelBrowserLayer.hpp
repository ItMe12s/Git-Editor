#pragma once

#include "common/ScrollListPopup.hpp"
#include "store/CommitStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <alphalaneous.alphas-ui-pack/include/API.hpp>

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

    void onClose(cocos2d::CCObject* sender) override;
    bool closeOnce(cocos2d::CCObject* sender = nullptr);
    void rebuildList();
    void renderList(std::vector<LevelSummary> levels);

    geode::Ref<LevelEditorLayer>    m_editor;
    geode::Ref<EditorPauseLayer>    m_pauseLayer;
    alpha::ui::AdvancedScrollLayer* m_scroll = nullptr;
    scroll_list_popup::ListState    m_listState{};
    bool m_busy = false;
};

} // namespace git_editor
