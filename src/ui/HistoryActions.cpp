#include "HistoryActions.hpp"

#include "HistoryLayer.hpp"
#include "editor/LevelStateIO.hpp"

#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <fmt/format.h>

namespace git_editor::history_actions {

bool canApplyEditorResult(LevelEditorLayer* editor) {
    return editor != nullptr && editor->getParent() != nullptr;
}

bool applyStateToEditorOrNotify(
    char const*       noun,
    LevelEditorLayer* editor,
    LevelState const& state,
    bool              hasConflicts
) {
    if (!canApplyEditorResult(editor)) {
        geode::Notification::create(
            (std::string(noun) + " ready but editor is no longer active, aborted before DB write").c_str(),
            geode::NotificationIcon::Warning
        )->show();
        return false;
    }
    if (!applyLevelState(editor, state)) {
        geode::Notification::create(
            (std::string(noun) + " ready but editor refused, aborted before DB write").c_str(),
            geode::NotificationIcon::Warning
        )->show();
        return false;
    }
    if (hasConflicts) {
        geode::Notification::create(
            (std::string(noun) + ": conflicts merged into editor state").c_str(),
            geode::NotificationIcon::Warning
        )->show();
    }
    return true;
}

void showConflictSummary(std::vector<Conflict> const& conflicts) {
    if (conflicts.empty()) return;

    int adds = 0, missing = 0, stale = 0;
    for (auto const& c : conflicts) {
        switch (c.kind) {
            case Conflict::Kind::AddAlreadyExists: ++adds;    break;
            case Conflict::Kind::Missing:          ++missing; break;
            case Conflict::Kind::ModifyStale:      ++stale;   break;
        }
    }

    std::string body = "Some ops could not be applied cleanly:\n";
    if (adds)    body += fmt::format("- {} add(s) already present\n", adds);
    if (missing) body += fmt::format("- {} missing\n", missing);
    if (stale)   body += fmt::format("- {} stale field(s) skipped", stale);

    FLAlertLayer::create(
        "Revert - partial",
        body.c_str(),
        "OK"
    )->show();
}

} // namespace git_editor::history_actions

namespace git_editor {

bool HistoryLayer::tryApplyToEditor(
    char const*       noun,
    LevelEditorLayer* editor,
    LevelState const& state,
    bool              hasConflicts
) {
    return history_actions::applyStateToEditorOrNotify(noun, editor, state, hasConflicts);
}

} // namespace git_editor
