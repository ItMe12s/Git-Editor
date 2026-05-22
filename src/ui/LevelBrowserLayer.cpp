#include "LevelBrowserLayer.hpp"

#include "HistoryActions.hpp"
#include "editor/LevelKey.hpp"
#include "editor/LevelStateIO.hpp"
#include "service/GitService.hpp"
#include "store/CommitStore.hpp"
#include "common/GitUiActionRunner.hpp"
#include "common/PreparedEditorFlow.hpp"
#include "common/ScrollListPopup.hpp"
#include "common/UiAction.hpp"
#include "common/UiNodeLifecycle.hpp"
#include "presentation/UiText.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/cocos.hpp>

#include <fmt/format.h>

#include <filesystem>
#include <string>
#include <system_error>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float browserRowHeight = 50.f;
constexpr float kRowMenuWidth    = 144.f;

} // namespace

LevelBrowserLayer* LevelBrowserLayer::create(
    LevelEditorLayer* editor,
    EditorPauseLayer*  pauseLayer
) {
    auto ret = new LevelBrowserLayer();
    if (ret && ret->init(editor, pauseLayer)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LevelBrowserLayer::init(
    LevelEditorLayer*  editor,
    EditorPauseLayer*  pauseLayer
) {
    if (!editor) {
        return false;
    }
    m_editor     = editor;
    m_pauseLayer = pauseLayer;

    if (!Popup::init(scroll_list_popup::Layout::kWidth, scroll_list_popup::Layout::kHeight)) {
        return false;
    }

    this->setTitle("Levels");

    {
        std::int64_t dbBytes = 0;
        std::error_code ec;
        auto sz = std::filesystem::file_size(sharedCommitStore().dbPath(), ec);
        if (!ec) dbBytes = static_cast<std::int64_t>(sz);
        auto totalLbl = CCLabelBMFont::create(
            ("Total: " + formatBytes(dbBytes)).c_str(), "chatFont.fnt"
        );
        totalLbl->setID("git-editor-levels-total"_spr);
        totalLbl->setScale(.5f);
        totalLbl->setAnchorPoint({1.f, .5f});
        totalLbl->setOpacity(200);
        m_mainLayer->addChildAtPosition(
            totalLbl, Anchor::TopRight, {-12.f, -16.f}
        );
    }

    m_scroll = scroll_list_popup::attachScrollList(
        this, m_mainLayer, "git-editor-levels-scroll"_spr
    );

    this->rebuildList();
    return true;
}

void LevelBrowserLayer::onClose(CCObject* sender) {
    scroll_list_popup::markClosing(m_listState, m_scroll);
    Popup::onClose(sender);
}

bool LevelBrowserLayer::closeOnce(CCObject* sender) {
    return scroll_list_popup::closeOnce(this, m_listState, sender, [this](CCObject* s) { onClose(s); });
}

void LevelBrowserLayer::rebuildList() {
    Ref<LevelBrowserLayer> self(this);
    scroll_list_popup::loadAsync<std::vector<LevelSummary>>(
        m_listState,
        m_scroll,
        "Loading levels...",
        "git-editor-levels-loading"_spr,
        []() { return sharedCommitStore().listLevels(); },
        [self](std::uint64_t serial) {
            return self && !scroll_list_popup::isStaleLoad(self->m_listState, serial);
        },
        [self](std::vector<LevelSummary> levels) mutable {
            self->renderList(std::move(levels));
        }
    );
}

void LevelBrowserLayer::renderList(std::vector<LevelSummary> levels) {
    if (m_listState.closing || !m_scroll) return;

    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();

    float const rowWidth = content->getContentSize().width;

    if (levels.empty()) {
        scroll_list_popup::showCenteredLabel(
            content, "No levels with commits.", "git-editor-levels-empty"_spr
        );
        scroll_list_popup::resetScrollTop(m_scroll);
        return;
    }

    Ref<LevelBrowserLayer> self(this);
    auto* editor = m_editor.data();
    if (!editor) return;
    std::string const destKey = levelKeyFor(editor->m_level);

    auto makeDeleteBtn = [](geode::Function<void(CCMenuItemSpriteExtra*)> cb)
        -> CCMenuItemSpriteExtra* {
        auto spr = ButtonSprite::create("Delete", "bigFont.fnt", "GJ_button_06.png", .8f);
        spr->setScale(.4f);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    };

    auto makeLoadBtn = [](geode::Function<void(CCMenuItemSpriteExtra*)> cb)
        -> CCMenuItemSpriteExtra* {
        auto spr = ButtonSprite::create("Load", "bigFont.fnt", "GJ_button_02.png", .8f);
        spr->setScale(.4f);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    };

    for (auto const& lv : levels) {
        auto row = CCNode::create();
        row->setID("git-editor-levels-row"_spr);
        row->setContentSize({rowWidth, browserRowHeight});
        row->setAnchorPoint({0.f, 0.f});
        row->setLayout(AnchorLayout::create());

        auto bg = CCLayerColor::create({0, 0, 0, 60}, rowWidth, browserRowHeight);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({.5f, .5f});
        row->addChildAtPosition(bg, Anchor::Center);

        auto keyLbl = CCLabelBMFont::create(shorten(lv.levelKey, 32).c_str(), "chatFont.fnt");
        keyLbl->setScale(.55f);
        keyLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(keyLbl, Anchor::Left, {6.f, 10.f});

        std::string sub = fmt::format(
            "{} commits - {} - ~{}",
            lv.commitCount,
            formatTimestamp(lv.lastCreatedAt),
            formatBytes(lv.totalBytes)
        );
        auto subLbl = CCLabelBMFont::create(sub.c_str(), "chatFont.fnt");
        subLbl->setScale(.45f);
        subLbl->setOpacity(200);
        subLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(subLbl, Anchor::Left, {6.f, -10.f});

        auto menu = CCMenu::create();
        menu->setID("git-editor-levels-row-menu"_spr);
        menu->setContentSize({kRowMenuWidth, browserRowHeight});
        menu->setAnchorPoint({1.f, .5f});
        menu->setLayout(
            RowLayout::create()
                ->setGap(4.f)
                ->setAxisAlignment(AxisAlignment::End)
                ->setCrossAxisOverflow(true)
        );

        auto const levelKey = lv.levelKey;
        auto const count    = lv.commitCount;

        auto loadBtn = makeLoadBtn(
            [self, levelKey, destKey, count](CCMenuItemSpriteExtra*) {
                if (!self || !tryBeginBusyAction(self->m_busy)) return;
                if (levelKey == destKey) {
                    finishBusyAction(self->m_busy);
                    Notification::create("Already this level: nothing to load", NotificationIcon::Info)
                        ->show();
                    return;
                }
                std::string const warnTitle = "ARE YOU SURE?";
                std::string const warnBody  =
                    "This will PERMANENTLY DELETE all objects and the commit history for your current level, "
                    "and overwrite it with the commit history from the selected level.\n"
                    "Make sure you're on an EMPTY level before doing this.\nThis CANNOT be undone.\n"
                    "Selected history: "
                    + std::to_string(count)
                    + " commit(s) from \"" + shorten(levelKey, 32) + "\".";

                createQuickPopup(
                    warnTitle.c_str(),
                    warnBody.c_str(),
                    "Cancel", "Load",
                    [self, levelKey, destKey](FLAlertLayer*, bool yes) {
                        if (!yes) {
                            if (self) finishBusyAction(self->m_busy);
                            return;
                        }
                        if (!self || !self->m_editor.data()) {
                            if (self) finishBusyAction(self->m_busy);
                            return;
                        }
                        Ref<LevelEditorLayer> editorRef(self->m_editor.data());
                        Ref<EditorPauseLayer> pauseRef(self->m_pauseLayer.data());
                        prepared_editor_flow::run<LevelState, PendingHistoryReplace, void>(
                            {self->m_busy, self->m_listState.closing},
                            [levelKey, destKey]() {
                                return sharedGitService().prepareImportLevelFrom(destKey, levelKey);
                            },
                            [editorRef](Prepared<LevelState> const& prep) {
                                auto* editor = editorRef.data();
                                if (!history_actions::canApplyEditorResult(editor)) {
                                    Notification::create(
                                        "Load ready but editor is no longer active; aborted before DB write",
                                        NotificationIcon::Warning
                                    )->show();
                                    return false;
                                }
                                if (!applyLevelState(editor, prep.result.value)) {
                                    Notification::create(
                                        "Editor refused load; aborted before DB write",
                                        NotificationIcon::Warning
                                    )->show();
                                    return false;
                                }
                                return true;
                            },
                            [](Prepared<LevelState> const& prep) { return prep.pendingReplace; },
                            [](PendingHistoryReplace pending, LevelState const& applied) {
                                return sharedGitService().finalizeImportLevelFrom(pending, applied);
                            },
                            prepared_editor_flow::OutcomeHandlers{
                                .onSuccess = [self, pauseRef]() {
                                    Notification::create("Level loaded", NotificationIcon::Success)->show();
                                    bool const closed = self->closeOnce(nullptr);
                                    prepared_editor_flow::resumePauseIfNeeded(
                                        pauseRef, closed || self->m_listState.closing
                                    );
                                },
                                .onPrepareError = [](std::string const& error) {
                                    Notification::create(
                                        ("Load failed: " + error).c_str(), NotificationIcon::Error
                                    )->show();
                                },
                                .onFinalizeError = [](std::string const& error) {
                                    Notification::create(
                                        ("Editor applied but history copy failed: " + error).c_str(),
                                        NotificationIcon::Error
                                    )->show();
                                },
                                .onAppliedOnly = []() {},
                            }
                        );
                    }
                );
            }
        );
        menu->addChild(loadBtn);

        auto delBtn = makeDeleteBtn([self, levelKey, count](CCMenuItemSpriteExtra*) {
            if (!self || !tryBeginBusyAction(self->m_busy)) return;
            createQuickPopup(
                "ARE YOU SURE?",
                ("PERMANENTLY DELETE all " + std::to_string(count) + " commits for \"" + shorten(levelKey, 48)
                 + "\"?\nThis CANNOT be undone.")
                    .c_str(),
                "Cancel", "Delete",
                [self, levelKey](FLAlertLayer*, bool yes) {
                    if (!yes) {
                        if (self) finishBusyAction(self->m_busy);
                        return;
                    }
                    if (!self) return;
                    Ref<LevelBrowserLayer> alive(self.data());
                    ui_action_runner::runWorkerResult<bool>(
                        [levelKey]() {
                            bool ok = sharedCommitStore().deleteLevel(levelKey);
                            if (ok) sharedGitService().clearReconstructCache();
                            return ok;
                        },
                        [alive](bool ok) {
                            if (!alive) return;
                            finishBusyAction(alive->m_busy);
                            if (alive->m_listState.closing) return;
                            if (!ok) {
                                Notification::create("Delete failed", NotificationIcon::Error)->show();
                                return;
                            }
                            Notification::create("Level history removed", NotificationIcon::Success)
                                ->show();
                            if (ui_node_lifecycle::isNodeActive(alive.data())) {
                                alive->rebuildList();
                            }
                        }
                    );
                }
            );
        });
        menu->addChild(delBtn);
        menu->updateLayout();
        row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});

        content->addChild(row);
    }

    content->updateLayout();
    scroll_list_popup::resetScrollTop(m_scroll);
}

} // namespace git_editor
