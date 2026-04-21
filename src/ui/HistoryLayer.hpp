#pragma once

#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <string>

namespace git_editor {

// Popup: scrollable list of commits for a level. Each row exposes two
// actions:
//   - Checkout: load that commit's state into the editor. Persists as a
//     new auto-revert commit so history is never lost.
//   - Revert: undo the operations introduced by that specific commit,
//     leaving every newer commit's work intact. Creates a new revert
//     commit; conflicts are surfaced in a summary dialog.
//
// Auto-revert and revert commits are rendered with a small badge so the
// user can tell real edits apart from state changes.
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
