#include "HistoryLayer.hpp"

#include "CommitMessageLayer.hpp"
#include "HistoryActions.hpp"
#include "common/PreparedEditorFlow.hpp"
#include "common/UiAction.hpp"
#include "common/UiNodeLifecycle.hpp"
#include "service/GitService.hpp"
#include "util/format/Shorten.hpp"

#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace git_editor {

namespace {

template <typename Row>
std::vector<CommitId> selectedOldestFirst(
    std::vector<Row> const& commitsDesc,
    std::set<CommitId> const& selected
) {
    std::vector<CommitId> ids;
    ids.reserve(selected.size());
    for (auto it = commitsDesc.rbegin(); it != commitsDesc.rend(); ++it) {
        if (selected.count(it->id)) {
            ids.push_back(it->id);
        }
    }
    return ids;
}

} // namespace

void HistoryLayer::onSquashPressed() {
    if (!tryBeginBusyAction(m_busy)) return;
    if (m_selected.size() < 2) {
        finishBusyAction(m_busy);
        Notification::create("Select at least 2 commits", NotificationIcon::Warning)->show();
        return;
    }

    auto const& commits = m_commits;

    auto idsOldestFirst = selectedOldestFirst(commits, m_selected);
    if (idsOldestFirst.size() != m_selected.size()) {
        finishBusyAction(m_busy);
        Notification::create("Selection mismatch", NotificationIcon::Error)->show();
        return;
    }

    std::vector<std::string> messagesOldestFirst;
    messagesOldestFirst.reserve(idsOldestFirst.size());
    for (auto id : idsOldestFirst) {
        auto it = std::find_if(commits.begin(), commits.end(),
            [id](CommitSummary const& row) { return row.id == id; });
        if (it != commits.end()) messagesOldestFirst.push_back(it->message);
    }

    std::string defaultMsg = "Squash: ";
    for (std::size_t i = 0; i < messagesOldestFirst.size(); ++i) {
        if (i) defaultMsg += ", ";
        defaultMsg += shorten(messagesOldestFirst[i], 20);
        if (defaultMsg.size() > 110) { defaultMsg += "..."; break; }
    }
    if (defaultMsg.size() > 120) defaultMsg.resize(120);

    Ref<HistoryLayer> self(this);
    createQuickPopup(
        "ARE YOU SURE?",
        "This will combine the selected commit range into a single commit.\nThis CANNOT be undone.",
        "Cancel", "Squash",
        [self, idsOldestFirst, defaultMsg](FLAlertLayer*, bool yes) mutable {
            if (!self || exitBusyIfClosing(self->m_busy, self->m_listState.closing)) return;
            if (!yes) { finishBusyAction(self->m_busy); return; }
            self->onSquashConfirmed(std::move(idsOldestFirst), std::move(defaultMsg));
        }
    );
}

void HistoryLayer::onSquashConfirmed(std::vector<CommitId> idsOldestFirst, std::string defaultMsg) {
    Ref<HistoryLayer> self(this);
    auto popup = CommitMessageLayer::create(
        [self, idsOldestFirst](std::string const& msg) {
            if (self && !self->m_listState.closing) self->runSquash(idsOldestFirst, msg);
        },
        "Squash Commits",
        "Squash",
        defaultMsg,
        [self]() { if (self && !self->m_listState.closing) finishBusyAction(self->m_busy); }
    );
    if (popup) popup->show();
    else       finishBusyAction(m_busy);
}

void HistoryLayer::runSquash(std::vector<CommitId> idsOldestFirst, std::string message) {
    Ref<HistoryLayer> self(this);
    Ref<LevelEditorLayer> editorRef(m_editor.data());
    std::string levelKey = m_levelKey;
    prepared_editor_flow::run<LevelState, PendingSquash, CommitId>(
        {self->m_busy, self->m_listState.closing},
        [levelKey, idsOldestFirst, message]() {
            return sharedGitService().prepareSquash(levelKey, idsOldestFirst, message);
        },
        [self, editorRef](Prepared<LevelState> const& prep) {
            return self->tryApplyToEditor("Squash", editorRef.data(), prep.result.value, false);
        },
        [](Prepared<LevelState> const& prep) { return prep.pendingSquash; },
        [](PendingSquash pending, LevelState const& applied) {
            return sharedGitService().finalizeSquash(pending, applied);
        },
        prepared_editor_flow::OutcomeHandlers{
            .onSuccess = [self]() {
                Notification::create("Squashed", NotificationIcon::Success)->show();
                self->m_squashMode = false;
                self->m_selected.clear();
                if (ui_node_lifecycle::isNodeActive(self.data())) {
                    self->rebuildHeader();
                    self->rebuildList();
                }
            },
            .onPrepareError = [](std::string const& error) {
                Notification::create(
                    ("Squash failed: " + error).c_str(), NotificationIcon::Error
                )->show();
            },
            .onFinalizeError = [](std::string const& error) {
                Notification::create(
                    ("Editor applied but DB squash failed: " + error).c_str(),
                    NotificationIcon::Error
                )->show();
            },
            .onAppliedOnly = []() {},
        }
    );
}

void HistoryLayer::startCheckoutFlow(CommitId commitId, std::string const& commitMsg) {
    if (!tryBeginBusyAction(m_busy)) return;
    Ref<HistoryLayer> self(this);
    Ref<LevelEditorLayer> editorRef(m_editor.data());
    Ref<EditorPauseLayer> pauseRef(m_pauseLayer.data());
    std::string levelKey = m_levelKey;
    createQuickPopup(
        "Checkout",
        ("Load state of commit \"" + shorten(commitMsg, 40) +
         "\"? A new auto-revert commit will be added on top of HEAD.").c_str(),
         "Cancel", "Checkout",
        [self, editorRef, pauseRef, levelKey, commitId](FLAlertLayer*, bool yes) {
            if (!yes) {
                if (self) finishBusyAction(self->m_busy);
                return;
            }
            if (!self) return;
            prepared_editor_flow::run<LevelState, PendingHeadUpdate, CommitId>(
                {self->m_busy, self->m_listState.closing},
                [levelKey, commitId]() {
                    return sharedGitService().prepareCheckout(levelKey, commitId);
                },
                [self, editorRef](Prepared<LevelState> const& prep) {
                    return self->tryApplyToEditor(
                        "Checkout", editorRef.data(), prep.result.value, false
                    );
                },
                [](Prepared<LevelState> const& prep) { return prep.pendingHead; },
                [](PendingHeadUpdate pending, LevelState const& applied) {
                    return sharedGitService().finalizeCheckout(pending, applied);
                },
                prepared_editor_flow::OutcomeHandlers{
                    .onSuccess = [self, pauseRef]() {
                        Notification::create("Checked out", NotificationIcon::Success)->show();
                        bool const closed = self->closeOnce(nullptr);
                        prepared_editor_flow::resumePauseIfNeeded(
                            pauseRef, closed || self->m_listState.closing
                        );
                    },
                    .onPrepareError = [](std::string const& error) {
                        Notification::create(
                            ("Checkout failed: " + error).c_str(), NotificationIcon::Error
                        )->show();
                    },
                    .onFinalizeError = [](std::string const& error) {
                        Notification::create(
                            ("Editor applied but DB write failed: " + error +
                             ". Re-commit to persist.").c_str(),
                            NotificationIcon::Error
                        )->show();
                    },
                }
            );
        }
    );
}

void HistoryLayer::startRevertFlow(CommitId commitId, std::string const& commitMsg) {
    if (!tryBeginBusyAction(m_busy)) return;
    Ref<HistoryLayer> self(this);
    Ref<LevelEditorLayer> editorRef(m_editor.data());
    Ref<EditorPauseLayer> pauseRef(m_pauseLayer.data());
    std::string levelKey = m_levelKey;
    createQuickPopup(
        "Revert",
        ("Undo just the changes from commit \"" + shorten(commitMsg, 40) +
         "\"? Later commits are preserved.").c_str(),
         "Cancel", "Revert",
        [self, editorRef, pauseRef, levelKey, commitId](FLAlertLayer*, bool yes) {
            if (!yes) {
                if (self) finishBusyAction(self->m_busy);
                return;
            }
            if (!self) return;
            auto conflicts = std::make_shared<std::vector<Conflict>>();
            prepared_editor_flow::run<RevertPayload, PendingHeadUpdate, CommitId>(
                {self->m_busy, self->m_listState.closing},
                [levelKey, commitId]() {
                    return sharedGitService().prepareRevert(levelKey, commitId);
                },
                [self, editorRef, conflicts](Prepared<RevertPayload> const& prep) {
                    *conflicts = prep.result.value.conflicts;
                    bool const hasConflicts = !conflicts->empty();
                    return self->tryApplyToEditor(
                        "Revert", editorRef.data(), prep.result.value.state, hasConflicts
                    );
                },
                [](Prepared<RevertPayload> const& prep) { return prep.pendingHead; },
                [](PendingHeadUpdate pending, RevertPayload const& payload) {
                    return sharedGitService().finalizeRevert(pending, payload.state);
                },
                prepared_editor_flow::OutcomeHandlers{
                    .onSuccess = [self, pauseRef, conflicts]() {
                        Notification::create("Reverted", NotificationIcon::Success)->show();
                        history_actions::showConflictSummary(*conflicts);
                        bool const closed = self->closeOnce(nullptr);
                        prepared_editor_flow::resumePauseIfNeeded(
                            pauseRef, closed || self->m_listState.closing
                        );
                    },
                    .onPrepareError = [](std::string const& error) {
                        Notification::create(
                            ("Revert failed: " + error).c_str(), NotificationIcon::Error
                        )->show();
                    },
                    .onFinalizeError = [](std::string const& error) {
                        Notification::create(
                            ("Editor applied but DB write failed: " + error +
                             ". Re-commit to persist.").c_str(),
                            NotificationIcon::Error
                        )->show();
                    },
                }
            );
        }
    );
}

} // namespace git_editor
