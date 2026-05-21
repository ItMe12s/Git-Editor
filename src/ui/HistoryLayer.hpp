#pragma once

#include "model/LevelState.hpp"
#include "store/CommitStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <alphalaneous.alphas-ui-pack/include/API.hpp>

#include <cocos2d.h>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace git_editor {

class HistoryLayer : public geode::Popup {
public:
    static HistoryLayer* create(
        std::string levelKey,
        LevelEditorLayer* editor,
        EditorPauseLayer* pauseLayer
    );

protected:
    bool init(std::string levelKey, LevelEditorLayer* editor, EditorPauseLayer* pauseLayer);

    void onClose(cocos2d::CCObject* sender) override;
    bool closeOnce(cocos2d::CCObject* sender = nullptr);
    void rebuildList();
    void renderList(std::vector<CommitSummary> commits);
    void rebuildHeader();
    void onSquashPressed();
    void onSquashConfirmed(std::vector<CommitId> idsOldestFirst, std::string defaultMsg);
    void runSquash(std::vector<CommitId> idsOldestFirst, std::string message);
    void startCheckoutFlow(CommitId commitId, std::string const& commitMsg);
    void startRevertFlow(CommitId commitId, std::string const& commitMsg);

    bool tryApplyToEditor(
        char const*       noun,
        LevelEditorLayer* editor,
        LevelState const& state,
        bool              hasConflicts
    );

    std::string                     m_levelKey;
    geode::Ref<LevelEditorLayer>    m_editor;
    geode::Ref<EditorPauseLayer>    m_pauseLayer;
    alpha::ui::AdvancedScrollLayer* m_scroll = nullptr;

    bool                       m_squashMode = false;
    bool                       m_busy       = false;
    bool                       m_closing    = false;
    std::uint64_t              m_loadSerial = 0;
    std::vector<CommitSummary> m_commits;
    std::set<CommitId>         m_selected;
    cocos2d::CCMenu*           m_headerMenu = nullptr;
};

} // namespace git_editor
