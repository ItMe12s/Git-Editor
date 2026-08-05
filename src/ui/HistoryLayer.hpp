#pragma once

#include "HistoryCommitRow.hpp"
#include "common/ScrollListPopup.hpp"
#include "store/CommitStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/async.hpp>
#include <alphalaneous.alphas-ui-pack/include/API.hpp>

#include <cocos2d.h>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace git_editor {

class HistoryLayer : public geode::Popup {
    friend cocos2d::CCNode* history_rows::createCommitRow(
        CommitSummary const& commit,
        float rowWidth,
        bool squashMode,
        bool selected,
        HistoryLayer* layer
    );

public:
    static HistoryLayer* create(
        std::string levelKey,
        LevelEditorLayer* editor,
        EditorPauseLayer* pauseLayer
    );

protected:
    struct HistoryLoadResult {
        LevelKey                   levelKey;
        std::vector<CommitSummary> commits;
    };

    bool init(std::string levelKey, LevelEditorLayer* editor, EditorPauseLayer* pauseLayer);

    void onClose(cocos2d::CCObject* sender) override;
    bool closeOnce(cocos2d::CCObject* sender = nullptr);
    static HistoryLoadResult loadHistory(LevelKey levelKey, LevelKey const& activeEditorLevelKey);
    void rebuildList();
    void renderList(std::vector<CommitSummary> commits);
    void rebuildHeader();
    void onSquashPressed();
    void onSquashConfirmed(std::vector<CommitId> idsOldestFirst, std::string defaultMsg);
    void runSquash(std::vector<CommitId> idsOldestFirst, std::string message);
    void startCheckoutFlow(CommitId commitId, std::string const& commitMsg);
    void startRevertFlow(CommitId commitId, std::string const& commitMsg);

    std::string                     m_levelKey;
    geode::Ref<LevelEditorLayer>    m_editor;
    geode::Ref<EditorPauseLayer>    m_pauseLayer;
    alpha::ui::AdvancedScrollLayer* m_scroll = nullptr;

    bool                         m_squashMode = false;
    bool                         m_busy       = false;
    scroll_list_popup::ListState m_listState{};
    std::vector<CommitSummary>   m_commits;
    std::set<CommitId>           m_selected;
    cocos2d::CCMenu*             m_headerMenu = nullptr;
    geode::async::TaskHolder<HistoryLoadResult> m_loadTask;
};

} // namespace git_editor
