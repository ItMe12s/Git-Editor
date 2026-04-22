#pragma once

#include "../diff/Delta.hpp"
#include "../store/CommitStore.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <cocos2d.h>
#include <set>
#include <string>
#include <unordered_map>

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
    void rebuildHeader();
    void onSquashPressed();

    std::string                     m_levelKey;
    LevelEditorLayer*               m_editor     = nullptr;
    EditorPauseLayer*               m_pauseLayer = nullptr;
    geode::ScrollLayer*             m_scroll     = nullptr;

    bool                            m_squashMode = false;
    bool                            m_busy       = false;
    std::set<CommitId>              m_selected;
    std::unordered_map<CommitId, DeltaStats> m_statsCache;
    cocos2d::CCMenu*       m_headerMenu = nullptr;
    CCMenuItemSpriteExtra* m_squashBtn  = nullptr;
};

} // namespace git_editor
