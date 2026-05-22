#include "HistoryLayer.hpp"

#include "HistoryActions.hpp"
#include "CommitMessageLayer.hpp"
#include "HistoryCommitRow.hpp"
#include "common/GitUiActionRunner.hpp"
#include "common/PreparedEditorFlow.hpp"
#include "common/ScrollListPopup.hpp"
#include "common/UiAction.hpp"
#include "common/UiNodeLifecycle.hpp"
#include "editor/LevelKey.hpp"
#include "editor/LevelStateIO.hpp"
#include "service/GitService.hpp"
#include "util/format/Shorten.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <memory>
#include <set>
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

constexpr float histPopupWidth    = scroll_list_popup::Layout::kWidth;
constexpr float histPopupHeight   = scroll_list_popup::Layout::kHeight;
struct HistoryLoadResult {
    LevelKey                   levelKey;
    std::vector<CommitSummary> commits;
};

HistoryLoadResult loadHistory(LevelKey levelKey, LevelKey const& activeEditorLevelKey) {
    auto commits = sharedGitService().listSummaries(levelKey);
    if (commits.empty() && !activeEditorLevelKey.empty() && activeEditorLevelKey != levelKey) {
        auto activeCommits = sharedGitService().listSummaries(activeEditorLevelKey);
        if (!activeCommits.empty()) {
            levelKey = activeEditorLevelKey;
            commits  = std::move(activeCommits);
        }
    }
    return { std::move(levelKey), std::move(commits) };
}

} // namespace

HistoryLayer* HistoryLayer::create(
    std::string levelKey,
    LevelEditorLayer* editor,
    EditorPauseLayer* pauseLayer
) {
    auto ret = new HistoryLayer();
    if (ret && ret->init(std::move(levelKey), editor, pauseLayer)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool HistoryLayer::init(
    std::string levelKey,
    LevelEditorLayer* editor,
    EditorPauseLayer* pauseLayer
) {
    if (!Popup::init(histPopupWidth, histPopupHeight)) return false;

    m_levelKey   = std::move(levelKey);
    m_editor     = editor;
    m_pauseLayer = pauseLayer;

    this->setTitle("History");

    m_scroll = scroll_list_popup::attachScrollList(
        this, m_mainLayer, "git-editor-history-scroll"_spr
    );

    m_headerMenu = CCMenu::create();
    m_headerMenu->setID("git-editor-history-header-menu"_spr);
    m_headerMenu->setContentSize({200.f, 26.f});
    m_headerMenu->setAnchorPoint({1.f, .5f});
    m_headerMenu->setLayout(
        RowLayout::create()
            ->setGap(6.f)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisOverflow(true)
    );
    m_mainLayer->addChildAtPosition(m_headerMenu, Anchor::TopRight, {-12.f, -16.f});

    this->rebuildHeader();
    this->rebuildList();
    return true;
}

void HistoryLayer::onClose(CCObject* sender) {
    scroll_list_popup::markClosing(m_listState, m_scroll);
    m_headerMenu = nullptr;
    Popup::onClose(sender);
}

bool HistoryLayer::closeOnce(CCObject* sender) {
    if (m_listState.closing || !ui_node_lifecycle::isNodeActive(this)) return false;
    this->onClose(sender);
    return true;
}

void HistoryLayer::rebuildHeader() {
    if (m_listState.closing || !m_headerMenu) return;
    m_headerMenu->removeAllChildren();

    Ref<HistoryLayer> self(this);

    auto modeLabel = m_squashMode ? "Exit Squash" : "Squash Mode";
    auto modeTex   = m_squashMode ? "GJ_button_06.png" : "GJ_button_04.png";
    auto modeSpr   = ButtonSprite::create(modeLabel, "bigFont.fnt", modeTex, .8f);
    modeSpr->setScale(.45f);
    auto modeBtn = CCMenuItemExt::createSpriteExtra(modeSpr,
        [self](CCMenuItemSpriteExtra*) {
            if (!self) return;
            self->m_squashMode = !self->m_squashMode;
            self->m_selected.clear();
            self->rebuildHeader();
            if (self->m_commits.empty()) self->rebuildList();
            else                         self->renderList(self->m_commits);
        }
    );
    modeBtn->setID("git-editor-history-mode-btn"_spr);
    m_headerMenu->addChild(modeBtn);

    if (m_squashMode && m_selected.size() >= 2) {
        auto label = std::string("Squash ") + std::to_string(m_selected.size());
        auto spr   = ButtonSprite::create(label.c_str(), "bigFont.fnt", "GJ_button_01.png", .8f);
        spr->setScale(.45f);
        auto squashBtn = CCMenuItemExt::createSpriteExtra(spr,
            [self](CCMenuItemSpriteExtra*) {
                if (self) self->onSquashPressed();
            }
        );
        squashBtn->setID("git-editor-history-squash-btn"_spr);
        m_headerMenu->addChild(squashBtn);
    }

    m_headerMenu->updateLayout();
}

void HistoryLayer::rebuildList() {
    if (m_listState.closing || !m_scroll) return;

    auto* editor = m_editor.data();
    auto const activeKey = (editor && editor->m_level) ? levelKeyFor(editor->m_level) : "";
    Ref<HistoryLayer> self(this);
    std::string levelKey = m_levelKey;

    scroll_list_popup::loadAsync<HistoryLoadResult>(
        m_listState,
        m_scroll,
        "Loading commits...",
        "git-editor-history-loading"_spr,
        [levelKey, activeKey]() { return loadHistory(levelKey, activeKey); },
        [self](std::uint64_t serial) {
            return self && !scroll_list_popup::isStaleLoad(self->m_listState, serial);
        },
        [self](HistoryLoadResult loaded) mutable {
            self->m_levelKey = std::move(loaded.levelKey);
            self->renderList(std::move(loaded.commits));
        }
    );
}

void HistoryLayer::renderList(std::vector<CommitSummary> loadedCommits) {
    if (m_listState.closing || !m_scroll) return;

    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();
    m_commits = std::move(loadedCommits);
    auto const& commits = m_commits;

    float const rowWidth = content->getContentSize().width;

    if (commits.empty()) {
        scroll_list_popup::showCenteredLabel(
            content, "No commits yet.", "git-editor-history-empty"_spr
        );
        scroll_list_popup::resetScrollTop(m_scroll);
        return;
    }

    Ref<HistoryLayer> self(this);

    for (auto const& c : commits) {
        bool const selected = m_squashMode && m_selected.count(c.id) > 0;
        content->addChild(history_rows::createCommitRow(
            c, rowWidth, m_squashMode, selected, self
        ));
    }

    content->updateLayout();
    scroll_list_popup::resetScrollTop(m_scroll);
}

bool HistoryLayer::tryApplyToEditor(
    char const*       noun,
    LevelEditorLayer* editor,
    LevelState const& state,
    bool              hasConflicts
) {
    return history_actions::applyStateToEditorOrNotify(noun, editor, state, hasConflicts);
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

} // namespace git_editor
