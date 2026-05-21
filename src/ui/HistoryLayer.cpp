#include "HistoryLayer.hpp"

#include "CommitMessageLayer.hpp"
#include "DeltaInfoLayer.hpp"
#include "HistoryActions.hpp"
#include "common/GitUiActionRunner.hpp"
#include "editor/LevelKey.hpp"
#include "editor/LevelStateIO.hpp"
#include "service/GitService.hpp"
#include "presentation/UiAction.hpp"
#include "presentation/UiText.hpp"

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

constexpr float histPopupWidth    = 420.f;
constexpr float histPopupHeight   = 280.f;
constexpr float histListPadX      = 20.f;
constexpr float histListPadTop    = 36.f;
constexpr float histListPadBottom = 16.f;
constexpr float histRowHeight     = 46.f;
constexpr ccColor3B kAddColor     = {64, 227, 72};
constexpr ccColor3B kModColor     = {50, 200, 255};
constexpr ccColor3B kDelColor     = {255, 90, 90};
constexpr ccColor3B kHdrColor     = {255, 210, 70};

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

    float const innerW = histPopupWidth  - histListPadX * 2.f;
    float const innerH = histPopupHeight - histListPadTop - histListPadBottom;

    m_scroll = alpha::ui::AdvancedScrollLayer::create({innerW, innerH});
    m_scroll->setID("git-editor-history-scroll"_spr);
    m_scroll->setLayout(
        ColumnLayout::create()
            ->setGap(3.f)
            ->setCrossAxisOverflow(false)
            ->setAutoGrowAxis(std::optional<float>(innerH))
    );

    m_mainLayer->addChildAtPosition(
        m_scroll, Anchor::Center,
        { -innerW * .5f, -innerH * .55f },
        /* useAnchorLayout */ false
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

void HistoryLayer::rebuildHeader() {
    if (!m_headerMenu) return;
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
            self->rebuildList();
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
    if (!m_scroll) return;

    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();

    auto* editor = m_editor.data();
    auto const activeKey = (editor && editor->m_level) ? levelKeyFor(editor->m_level) : "";
    auto loading = CCLabelBMFont::create("Loading commits...", "bigFont.fnt");
    loading->setID("git-editor-history-loading"_spr);
    loading->setScale(.5f);
    loading->setOpacity(160);
    content->addChild(loading);
    content->updateLayout();

    auto const serial = ++m_loadSerial;
    Ref<HistoryLayer> self(this);
    std::string levelKey = m_levelKey;
    ui_action_runner::runWorkerResult<HistoryLoadResult>(
        [levelKey, activeKey]() {
            return loadHistory(levelKey, activeKey);
        },
        [self, serial](HistoryLoadResult loaded) mutable {
            if (!self || serial != self->m_loadSerial) return;
            self->m_levelKey = std::move(loaded.levelKey);
            self->renderList(std::move(loaded.commits));
        }
    );
}

void HistoryLayer::renderList(std::vector<CommitSummary> loadedCommits) {
    if (!m_scroll) return;

    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();
    m_commits = std::move(loadedCommits);
    auto const& commits = m_commits;

    float const rowWidth = content->getContentSize().width;

    if (commits.empty()) {
        auto empty = CCLabelBMFont::create("No commits yet.", "bigFont.fnt");
        empty->setID("git-editor-history-empty"_spr);
        empty->setScale(.5f);
        empty->setOpacity(160);
        content->addChild(empty);
        content->updateLayout();
        m_scroll->setScrollY(0);
        return;
    }

    Ref<HistoryLayer> self(this);

    auto makeBtn = [](char const* label, char const* texture,
                      geode::Function<void(CCMenuItemSpriteExtra*)> cb) -> CCMenuItemSpriteExtra* {
        auto spr = ButtonSprite::create(label, "bigFont.fnt", texture, .8f);
        spr->setScale(.4f);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    };

    for (auto const& c : commits) {
        auto row = CCNode::create();
        row->setID("git-editor-history-row"_spr);
        row->setContentSize({rowWidth, histRowHeight});
        row->setAnchorPoint({0.f, 0.f});
        row->setLayout(AnchorLayout::create());

        auto bg = CCLayerColor::create({0, 0, 0, 60}, rowWidth, histRowHeight);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({.5f, .5f});
        row->addChildAtPosition(bg, Anchor::Center);

        auto timeLbl = CCLabelBMFont::create(
            formatTimestamp(c.createdAt).c_str(), "chatFont.fnt"
        );
        timeLbl->setScale(.5f);
        timeLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(timeLbl, Anchor::Left, {6.f, 11.f});

        auto statsNode = CCNode::create();
        statsNode->setContentSize({110.f, 12.f});
        statsNode->setAnchorPoint({0.f, .5f});
        statsNode->setLayout(
            RowLayout::create()
                ->setGap(4.f)
                ->setAxisAlignment(AxisAlignment::Start)
                ->setCrossAxisOverflow(false)
        );
        {
            auto makeStat = [](std::string const& text, ccColor3B color) {
                auto* lbl = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
                lbl->setScale(.45f);
                lbl->setColor(color);
                return lbl;
            };
            if (c.headerCount > 0) {
                statsNode->addChild(makeStat("h" + std::to_string(c.headerCount), kHdrColor));
            }
            if (c.addCount > 0) {
                statsNode->addChild(makeStat("+" + std::to_string(c.addCount), kAddColor));
            }
            if (c.modifyCount > 0) {
                statsNode->addChild(makeStat("~" + std::to_string(c.modifyCount), kModColor));
            }
            if (c.removeCount > 0) {
                statsNode->addChild(makeStat("-" + std::to_string(c.removeCount), kDelColor));
            }
        }
        statsNode->updateLayout();
        float const timeWidth = timeLbl->getContentSize().width * timeLbl->getScale();
        row->addChildAtPosition(statsNode, Anchor::Left, {10.f + timeWidth, 11.f});

        auto msgLbl = CCLabelBMFont::create(
            shorten(c.message, 34).c_str(), "chatFont.fnt"
        );
        msgLbl->setScale(.55f);
        msgLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(msgLbl, Anchor::Left, {6.f, -8.f});

        auto menu = CCMenu::create();
        menu->setID("git-editor-history-row-menu"_spr);
        menu->setContentSize({210.f, histRowHeight});
        menu->setAnchorPoint({1.f, .5f});
        menu->setLayout(
            RowLayout::create()
                ->setGap(4.f)
                ->setAxisAlignment(AxisAlignment::End)
                ->setCrossAxisOverflow(true)
        );

        auto const commitId  = c.id;
        auto const commitMsg = c.message;

        if (m_squashMode) {
            bool const checked = m_selected.count(commitId) > 0;
            auto tickBtn = CCMenuItemExt::createTogglerWithStandardSprites(
                .6f,
                [self, commitId](CCMenuItemToggler*) {
                    if (!self) return;
                    // Track m_selected, not isToggled(). GD binding mismatch.
                    if (self->m_selected.count(commitId)) self->m_selected.erase(commitId);
                    else                                  self->m_selected.insert(commitId);
                    self->rebuildHeader();
                }
            );
            tickBtn->toggle(checked);
            menu->addChild(tickBtn);

            menu->updateLayout();
            row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});
            content->addChild(row);
            continue;
        }

        auto helpBtn = makeBtn(
            "?", "GJ_button_04.png",
            [commitId, commitMsg](CCMenuItemSpriteExtra*) {
                auto res = sharedGitService().describeCommitChanges(commitId);
                if (!res.ok) {
                    FLAlertLayer::create("Error", res.error, "OK")->show();
                    return;
                }
                std::string title = "What changed";
                if (!commitMsg.empty()) {
                    title += " - ";
                    title += shorten(commitMsg, 24);
                }
                if (auto* p = DeltaInfoLayer::create(std::move(title), std::move(res.value))) {
                    p->show();
                }
            }
        );
        menu->addChild(helpBtn);

        auto renameBtn = makeBtn(
            "Rename", "GJ_button_04.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (auto popup = CommitMessageLayer::create(
                    [self, commitId](std::string const& newMessage) {
                        ui_action_runner::runWorkerResult<bool>(
                            [commitId, newMessage]() {
                                return sharedGitService().updateCommitMessage(commitId, newMessage);
                            },
                            [self](bool ok) {
                                if (!ok) {
                                    Notification::create("Rename failed", NotificationIcon::Error)->show();
                                    return;
                                }
                                Notification::create("Renamed commit", NotificationIcon::Success)->show();
                                if (self) self->rebuildList();
                            }
                        );
                    },
                    "Rename Commit",
                    "Save",
                    commitMsg
                )) {
                    popup->show();
                }
            }
        );
        menu->addChild(renameBtn);

        auto checkoutBtn = makeBtn(
            "Checkout", "GJ_button_02.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (self) self->startCheckoutFlow(commitId, commitMsg);
            }
        );
        menu->addChild(checkoutBtn);

        auto revertBtn = makeBtn(
            "Revert", "GJ_button_06.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (self) self->startRevertFlow(commitId, commitMsg);
            }
        );
        menu->addChild(revertBtn);

        menu->updateLayout();
        row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});

        content->addChild(row);
    }

    content->updateLayout();
    m_scroll->setScrollY(0);
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
            ui_action_runner::runWorkerResult<Prepared<LevelState>>(
                [levelKey, commitId]() {
                    return sharedGitService().prepareCheckout(levelKey, commitId);
                },
                [self, editorRef, pauseRef](Prepared<LevelState> prep) mutable {
                    if (!self) return;
                    if (!prep.result.ok) {
                        finishBusyAction(self->m_busy);
                        Notification::create(
                            ("Checkout failed: " + prep.result.error).c_str(), NotificationIcon::Error
                        )->show();
                        return;
                    }
                    if (!self->tryApplyToEditor("Checkout", editorRef.data(), prep.result.value, false)) {
                        finishBusyAction(self->m_busy);
                        return;
                    }
                    if (!prep.pendingHead) {
                        finishBusyAction(self->m_busy);
                        Notification::create("Checked out", NotificationIcon::Success)->show();
                        self->onClose(nullptr);
                        if (pauseRef.data()) pauseRef.data()->onResume(nullptr);
                        return;
                    }
                    LevelState applied = prep.result.value;
                    PendingHeadUpdate pending = *prep.pendingHead;
                    Ref<EditorPauseLayer> pauseLocal = pauseRef;
                    ui_action_runner::runWorkerResult<Result<CommitId>>(
                        [pending, applied]() {
                            return sharedGitService().finalizeCheckout(pending, applied);
                        },
                        [self, pauseLocal](Result<CommitId> fin) mutable {
                            if (self) finishBusyAction(self->m_busy);
                            if (!fin.ok) {
                                Notification::create(
                                    ("Editor applied but DB write failed: " + fin.error + ". Re-commit to persist.").c_str(),
                                    NotificationIcon::Error
                                )->show();
                                return;
                            }
                            Notification::create("Checked out", NotificationIcon::Success)->show();
                            if (self) self->onClose(nullptr);
                            if (pauseLocal.data()) pauseLocal.data()->onResume(nullptr);
                        }
                    );
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
            ui_action_runner::runWorkerResult<Prepared<RevertPayload>>(
                [levelKey, commitId]() {
                    return sharedGitService().prepareRevert(levelKey, commitId);
                },
                [self, editorRef, pauseRef](Prepared<RevertPayload> prep) mutable {
                    if (!self) return;
                    if (!prep.result.ok) {
                        finishBusyAction(self->m_busy);
                        Notification::create(
                            ("Revert failed: " + prep.result.error).c_str(), NotificationIcon::Error
                        )->show();
                        return;
                    }
                    bool const hasConflicts = !prep.result.value.conflicts.empty();
                    if (!self->tryApplyToEditor("Revert", editorRef.data(), prep.result.value.state, hasConflicts)) {
                        finishBusyAction(self->m_busy);
                        return;
                    }
                    if (!prep.pendingHead) {
                        finishBusyAction(self->m_busy);
                        Notification::create("Reverted", NotificationIcon::Success)->show();
                        history_actions::showConflictSummary(prep.result.value.conflicts);
                        self->onClose(nullptr);
                        if (pauseRef.data()) pauseRef.data()->onResume(nullptr);
                        return;
                    }
                    LevelState applied = prep.result.value.state;
                    PendingHeadUpdate pending = *prep.pendingHead;
                    std::vector<Conflict> conflicts = prep.result.value.conflicts;
                    Ref<EditorPauseLayer> pauseLocal = pauseRef;
                    ui_action_runner::runWorkerResult<Result<CommitId>>(
                        [pending, applied]() {
                            return sharedGitService().finalizeRevert(pending, applied);
                        },
                        [self, pauseLocal, conflicts](Result<CommitId> fin) mutable {
                            if (self) finishBusyAction(self->m_busy);
                            if (!fin.ok) {
                                Notification::create(
                                    ("Editor applied but DB write failed: " + fin.error + ". Re-commit to persist.").c_str(),
                                    NotificationIcon::Error
                                )->show();
                                return;
                            }
                            Notification::create("Reverted", NotificationIcon::Success)->show();
                            history_actions::showConflictSummary(conflicts);
                            if (self) self->onClose(nullptr);
                            if (pauseLocal.data()) pauseLocal.data()->onResume(nullptr);
                        }
                    );
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
            if (!self) return;
            if (!yes) { finishBusyAction(self->m_busy); return; }
            self->onSquashConfirmed(std::move(idsOldestFirst), std::move(defaultMsg));
        }
    );
}

void HistoryLayer::onSquashConfirmed(std::vector<CommitId> idsOldestFirst, std::string defaultMsg) {
    Ref<HistoryLayer> self(this);
    auto popup = CommitMessageLayer::create(
        [self, idsOldestFirst](std::string const& msg) {
            if (self) self->runSquash(idsOldestFirst, msg);
        },
        "Squash Commits",
        "Squash",
        defaultMsg,
        [self]() { if (self) finishBusyAction(self->m_busy); }
    );
    if (popup) popup->show();
    else       finishBusyAction(m_busy);
}

void HistoryLayer::runSquash(std::vector<CommitId> idsOldestFirst, std::string message) {
    Ref<HistoryLayer> self(this);
    Ref<LevelEditorLayer> editorRef(m_editor.data());
    Ref<EditorPauseLayer> pauseRef(m_pauseLayer.data());
    std::string levelKey = m_levelKey;
    ui_action_runner::runWorkerResult<Prepared<LevelState>>(
        [levelKey, idsOldestFirst, message]() {
            return sharedGitService().prepareSquash(levelKey, idsOldestFirst, message);
        },
        [self, editorRef, pauseRef](Prepared<LevelState> prep) mutable {
            if (!self) return;
            if (!prep.result.ok) {
                finishBusyAction(self->m_busy);
                Notification::create(
                    ("Squash failed: " + prep.result.error).c_str(), NotificationIcon::Error
                )->show();
                return;
            }
            if (!self->tryApplyToEditor("Squash", editorRef.data(), prep.result.value, false)) {
                finishBusyAction(self->m_busy);
                return;
            }
            if (!prep.pendingSquash) {
                finishBusyAction(self->m_busy);
                return;
            }
            LevelState applied = prep.result.value;
            PendingSquash pending = *prep.pendingSquash;
            ui_action_runner::runWorkerResult<Result<CommitId>>(
                [pending, applied]() {
                    return sharedGitService().finalizeSquash(pending, applied);
                },
                [self](Result<CommitId> fin) mutable {
                    if (self) finishBusyAction(self->m_busy);
                    if (!fin.ok) {
                        Notification::create(
                            ("Editor applied but DB squash failed: " + fin.error).c_str(),
                            NotificationIcon::Error
                        )->show();
                        return;
                    }
                    Notification::create("Squashed", NotificationIcon::Success)->show();
                    if (!self) return;
                    self->m_squashMode = false;
                    self->m_selected.clear();
                    self->rebuildHeader();
                    self->rebuildList();
                }
            );
        }
    );
}

} // namespace git_editor
