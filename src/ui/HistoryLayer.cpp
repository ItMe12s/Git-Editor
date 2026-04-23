#include "HistoryLayer.hpp"

#include "CommitMessageLayer.hpp"
#include "DeltaInfoLayer.hpp"
#include "common/GitUiActionRunner.hpp"
#include "history/HistoryDataSource.hpp"
#include "history/HistorySelectionModel.hpp"
#include "../diff/Delta.hpp"
#include "../editor/LevelStateIO.hpp"
#include "../service/GitService.hpp"
#include "../store/CommitStore.hpp"
#include "../util/LevelKey.hpp"
#include "../util/UiAction.hpp"
#include "../util/UiText.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/cocos.hpp>

#include <cerrno>
#include <algorithm>
#include <cstdio>
#include <vector>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float kPopupWidth    = 420.f;
constexpr float kPopupHeight   = 280.f;
constexpr float kListPadX      = 20.f;
constexpr float kListPadTop    = 36.f;
constexpr float kListPadBottom = 16.f;
constexpr float kRowHeight     = 46.f;
constexpr ccColor3B kAddColor  = {64, 227, 72};
constexpr ccColor3B kModColor  = {50, 200, 255};
constexpr ccColor3B kDelColor  = {255, 90, 90};
constexpr ccColor3B kHdrColor  = {255, 210, 70};

bool canApplyEditorResult(LevelEditorLayer* editor) {
    return editor != nullptr && editor->getParent() != nullptr;
}

void showConflictSummary(std::vector<Conflict> const& conflicts) {
    if (conflicts.empty()) return;

    int adds = 0, missing = 0, stale = 0;
    for (auto const& c : conflicts) {
        switch (c.kind) {
            case Conflict::Kind::AddAlreadyExists: ++adds;   break;
            case Conflict::Kind::Missing:          ++missing; break;
            case Conflict::Kind::ModifyStale:      ++stale;   break;
        }
    }

    std::string body = "Some ops could not be applied cleanly:\n";
    if (adds)    body += "- " + std::to_string(adds)    + " add(s) already present\n";
    if (missing) body += "- " + std::to_string(missing) + " missing\n";
    if (stale)   body += "- " + std::to_string(stale)   + " stale field(s) skipped";

    FLAlertLayer::create(
        "Revert - partial",
        body.c_str(),
        "OK"
    )->show();
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
    if (!Popup::init(kPopupWidth, kPopupHeight)) return false;

    m_levelKey   = std::move(levelKey);
    m_editor     = editor;
    m_pauseLayer = pauseLayer;

    this->setTitle("History");

    float const innerW = kPopupWidth  - kListPadX * 2.f;
    float const innerH = kPopupHeight - kListPadTop - kListPadBottom;

    m_scroll = ScrollLayer::create({innerW, innerH});
    m_scroll->setAnchorPoint({0.f, 0.f});
    m_scroll->m_contentLayer->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setGap(3.f)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisOverflow(false)
            ->setAutoGrowAxis(std::optional<float>(innerH))
    );

    m_mainLayer->addChildAtPosition(
        m_scroll, Anchor::Center,
        { -innerW * .5f, -innerH * .55f },
        /* useAnchorLayout */ false
    );

    m_headerMenu = CCMenu::create();
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
    m_squashBtn = nullptr;

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
    m_headerMenu->addChild(modeBtn);

    if (m_squashMode && m_selected.size() >= 2) {
        auto label = std::string("Squash ") + std::to_string(m_selected.size());
        auto spr   = ButtonSprite::create(label.c_str(), "bigFont.fnt", "GJ_button_01.png", .8f);
        spr->setScale(.45f);
        m_squashBtn = CCMenuItemExt::createSpriteExtra(spr,
            [self](CCMenuItemSpriteExtra*) {
                if (self) self->onSquashPressed();
            }
        );
        m_headerMenu->addChild(m_squashBtn);
    }

    m_headerMenu->updateLayout();
}

void HistoryLayer::rebuildList() {
    if (!m_scroll) return;

    auto* content = m_scroll->m_contentLayer;
    content->removeAllChildren();

    auto const activeKey = (m_editor && m_editor->m_level) ? levelKeyFor(m_editor->m_level) : "";
    auto loaded = history_data_source::loadHistory(m_levelKey, activeKey);
    m_levelKey = loaded.levelKey;
    auto commits = std::move(loaded.commits);

    float const rowWidth = content->getContentSize().width;

    if (commits.empty()) {
        auto empty = CCLabelBMFont::create("No commits yet.", "bigFont.fnt");
        empty->setScale(.5f);
        empty->setOpacity(160);
        content->addChild(empty);
        content->updateLayout();
        m_scroll->scrollToTop();
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
        row->setContentSize({rowWidth, kRowHeight});
        row->setAnchorPoint({0.f, 0.f});
        row->setLayout(AnchorLayout::create());

        auto bg = CCLayerColor::create({0, 0, 0, 60}, rowWidth, kRowHeight);
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
        menu->setContentSize({210.f, kRowHeight});
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
                    // Track state via m_selected, not isToggled() (GD / binding mismatch).
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
                auto row = sharedCommitStore().get(commitId);
                if (!row) {
                    FLAlertLayer::create("Error", "Commit not found.", "OK")->show();
                    return;
                }
                if (auto opt = parseDelta(row->deltaBlob)) {
                    std::string body  = describeDeltaText(*opt);
                    std::string title = "What changed";
                    if (!commitMsg.empty()) {
                        title += " - ";
                        title += shorten(commitMsg, 24);
                    }
                    if (auto* p = DeltaInfoLayer::create(std::move(title), std::move(body))) {
                        p->show();
                    }
                } else {
                    FLAlertLayer::create(
                        "Error",
                        "Could not read this commit's delta.",
                        "OK"
                    )->show();
                }
            }
        );
        menu->addChild(helpBtn);

        auto renameBtn = makeBtn(
            "Rename", "GJ_button_04.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (auto popup = CommitMessageLayer::create(
                    [self, commitId](std::string const& newMessage) {
                        if (!sharedCommitStore().updateMessage(commitId, newMessage)) {
                            Notification::create("Rename failed", NotificationIcon::Error)->show();
                            return;
                        }
                        Notification::create("Renamed commit", NotificationIcon::Success)->show();
                        if (self) self->rebuildList();
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
    m_scroll->scrollToTop();
}

void HistoryLayer::startCheckoutFlow(CommitId commitId, std::string const& commitMsg) {
    if (!tryBeginBusyAction(m_busy)) return;
    Ref<HistoryLayer> self(this);
    Ref<LevelEditorLayer> editorRef(m_editor);
    Ref<EditorPauseLayer> pauseRef(m_pauseLayer);
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
            Ref<HistoryLayer> alive(self.data());
            ui_action_runner::runWorkerResult<CheckoutOutcome>(
                [levelKey, commitId]() {
                    return sharedGitService().checkout(levelKey, commitId);
                },
                [alive, editorRef, pauseRef](CheckoutOutcome outcome) mutable {
                    if (!alive) return;
                    finishBusyAction(alive->m_busy);
                    if (!outcome.ok) {
                        Notification::create(
                            ("Checkout failed: " + outcome.error).c_str(),
                            NotificationIcon::Error
                        )->show();
                        return;
                    }
                    auto* editor = editorRef.data();
                    auto* pauseLayer = pauseRef.data();
                    if (!canApplyEditorResult(editor)) {
                        Notification::create(
                            "Checkout succeeded but editor is no longer active",
                            NotificationIcon::Warning
                        )->show();
                        return;
                    }
                    if (!applyLevelState(editor, outcome.state)) {
                        Notification::create(
                            "Checkout applied to DB but editor refused",
                            NotificationIcon::Warning
                        )->show();
                    } else {
                        Notification::create("Checked out", NotificationIcon::Success)->show();
                    }
                    alive->onClose(nullptr);
                    if (pauseLayer) pauseLayer->onResume(nullptr);
                }
            );
        }
    );
}

void HistoryLayer::startRevertFlow(CommitId commitId, std::string const& commitMsg) {
    if (!tryBeginBusyAction(m_busy)) return;
    Ref<HistoryLayer> self(this);
    Ref<LevelEditorLayer> editorRef(m_editor);
    Ref<EditorPauseLayer> pauseRef(m_pauseLayer);
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
            Ref<HistoryLayer> alive(self.data());
            ui_action_runner::runWorkerResult<RevertOutcome>(
                [levelKey, commitId]() {
                    return sharedGitService().revert(levelKey, commitId);
                },
                [alive, editorRef, pauseRef](RevertOutcome outcome) mutable {
                    if (!alive) return;
                    finishBusyAction(alive->m_busy);
                    if (!outcome.ok) {
                        Notification::create(
                            ("Revert failed: " + outcome.error).c_str(),
                            NotificationIcon::Error
                        )->show();
                        return;
                    }
                    auto* editor = editorRef.data();
                    auto* pauseLayer = pauseRef.data();
                    if (!canApplyEditorResult(editor)) {
                        Notification::create(
                            "Revert succeeded but editor is no longer active",
                            NotificationIcon::Warning
                        )->show();
                        return;
                    }
                    if (!applyLevelState(editor, outcome.state)) {
                        Notification::create(
                            "Revert applied to DB but editor refused",
                            NotificationIcon::Warning
                        )->show();
                    } else if (outcome.conflicts.empty()) {
                        Notification::create("Reverted", NotificationIcon::Success)->show();
                    } else {
                        Notification::create(
                            "Reverted with conflicts", NotificationIcon::Warning
                        )->show();
                    }
                    alive->onClose(nullptr);
                    if (pauseLayer) pauseLayer->onResume(nullptr);
                    showConflictSummary(outcome.conflicts);
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

    auto commits = sharedCommitStore().listSummaries(m_levelKey);

    // commits is DESC by createdAt, build oldest-first selected list.
    std::vector<CommitId>    idsOldestFirst;
    std::vector<std::string> messagesOldestFirst;
    messagesOldestFirst.reserve(m_selected.size());
    idsOldestFirst = history_selection_model::selectedOldestFirst(commits, m_selected);
    for (auto id : idsOldestFirst) {
        auto it = std::find_if(commits.begin(), commits.end(), [id](CommitSummary const& row) { return row.id == id; });
        if (it != commits.end()) messagesOldestFirst.push_back(it->message);
    }

    if (idsOldestFirst.size() != m_selected.size()) {
        finishBusyAction(m_busy);
        Notification::create("Selection mismatch", NotificationIcon::Error)->show();
        return;
    }

    std::string defaultMsg = "Squash: ";
    for (std::size_t i = 0; i < messagesOldestFirst.size(); ++i) {
        if (i) defaultMsg += ", ";
        defaultMsg += shorten(messagesOldestFirst[i], 20);
        if (defaultMsg.size() > 110) {
            defaultMsg += "...";
            break;
        }
    }
    if (defaultMsg.size() > 120) defaultMsg.resize(120);

    auto* editor     = m_editor;
    auto* pauseLayer = m_pauseLayer;
    std::string levelKey = m_levelKey;
    Ref<HistoryLayer> self(this);
    Ref<LevelEditorLayer> editorRef(editor);
    Ref<EditorPauseLayer> pauseRef(pauseLayer);

    createQuickPopup(
        "ARE YOU SURE?",
        "This will combine the selected commit range into a single commit.\nThis CANNOT be undone.",
        "Cancel", "Squash",
        [self, editorRef, pauseRef, levelKey, idsOldestFirst, defaultMsg](
            FLAlertLayer*, bool yes) {
            if (!yes) {
                if (self) finishBusyAction(self->m_busy);
                return;
            }
            if (auto popup = CommitMessageLayer::create(
                [self, editorRef, pauseRef, levelKey, idsOldestFirst](std::string const& msg) {
                    if (!self) return;
                    Ref<HistoryLayer> alive(self.data());
                    ui_action_runner::runWorkerResult<SquashOutcome>(
                        [levelKey, idsOldestFirst, msg]() {
                            return sharedGitService().squash(levelKey, idsOldestFirst, msg);
                        },
                        [alive, editorRef, pauseRef](SquashOutcome outcome) mutable {
                                if (!alive) return;
                                finishBusyAction(alive->m_busy);
                                if (!outcome.ok) {
                                    Notification::create(
                                        ("Squash failed: " + outcome.error).c_str(),
                                        NotificationIcon::Error
                                    )->show();
                                    return;
                                }
                                auto* editor = editorRef.data();
                                auto* pauseLayer = pauseRef.data();
                                if (!canApplyEditorResult(editor)) {
                                    Notification::create(
                                        "Squash succeeded but editor is no longer active",
                                        NotificationIcon::Warning
                                    )->show();
                                    return;
                                }
                                if (!applyLevelState(editor, outcome.state)) {
                                    Notification::create(
                                        "Squash applied to DB but editor refused",
                                        NotificationIcon::Warning
                                    )->show();
                                } else {
                                    Notification::create("Squashed", NotificationIcon::Success)->show();
                                }
                                alive->m_squashMode = false;
                                alive->m_selected.clear();
                                alive->rebuildHeader();
                                alive->rebuildList();
                                (void)pauseLayer;
                        }
                    );
                },
                "Squash Commits",
                "Squash",
                defaultMsg,
                [self]() {
                    if (self) finishBusyAction(self->m_busy);
                }
            )) {
                popup->show();
            }
            else if (self) {
                finishBusyAction(self->m_busy);
            }
        }
    );
}

} // namespace git_editor
