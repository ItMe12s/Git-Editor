#pragma once

#include "../diff/Delta.hpp"
#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <cocos2d.h>
#include <set>
#include <string>
#include <vector>

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
    void onSquashConfirmed(std::vector<CommitId> idsOldestFirst, std::string defaultMsg);
    void runSquash(std::vector<CommitId> idsOldestFirst, std::string message);
    void startCheckoutFlow(CommitId commitId, std::string const& commitMsg);
    void startRevertFlow(CommitId commitId, std::string const& commitMsg);

    // Common post-worker step: apply state to editor (or warn), notify, close, resume.
    // pastTense: "Checked out"/"Reverted"/"Squashed" - notification on success.
    // noun: "Checkout"/"Revert"/"Squash" - prefix for warnings.
    void applyAndNotify(
        char const*       noun,
        char const*       pastTense,
        LevelEditorLayer* editor,
        EditorPauseLayer* pauseLayer,
        LevelState const& state,
        bool              hasConflicts,
        bool              closeAndResume
    );

    std::string                     m_levelKey;
    LevelEditorLayer*               m_editor     = nullptr;
    EditorPauseLayer*               m_pauseLayer = nullptr;
    geode::ScrollLayer*             m_scroll     = nullptr;

    bool                            m_squashMode = false;
    bool                            m_busy       = false;
    std::set<CommitId>              m_selected;
    cocos2d::CCMenu*       m_headerMenu = nullptr;
    CCMenuItemSpriteExtra* m_squashBtn  = nullptr;
};

} // namespace git_editor
