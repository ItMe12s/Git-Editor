#include "LevelBrowserLayer.hpp"

#include "../editor/LevelStateIO.hpp"
#include "../service/GitService.hpp"
#include "../store/CommitStore.hpp"
#include "../util/AsyncQueue.hpp"
#include "../util/UiAction.hpp"
#include "../util/LevelKey.hpp"
#include "../util/UiText.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/cocos.hpp>

#include <string>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float kPopupWidth     = 420.f;
constexpr float kPopupHeight    = 280.f;
constexpr float kListPadX       = 20.f;
constexpr float kListPadTop     = 36.f;
constexpr float kListPadBottom  = 16.f;
constexpr float kRowHeight      = 50.f;
constexpr float kRowMenuWidth   = 144.f;

bool canApplyEditorResult(LevelEditorLayer* editor) {
    return editor != nullptr && editor->getParent() != nullptr;
}

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

    if (!Popup::init(kPopupWidth, kPopupHeight)) {
        return false;
    }

    this->setTitle("Levels");

    float const innerW = kPopupWidth - kListPadX * 2.f;
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
        {-innerW * .5f, -innerH * .55f},
        false
    );

    this->rebuildList();
    return true;
}

void LevelBrowserLayer::rebuildList() {
    if (!m_scroll) return;

    auto* content = m_scroll->m_contentLayer;
    content->removeAllChildren();

    auto levels = sharedCommitStore().listLevels();
    float const rowWidth = content->getContentSize().width;

    if (levels.empty()) {
        auto empty = CCLabelBMFont::create("No levels with commits.", "bigFont.fnt");
        empty->setScale(.5f);
        empty->setOpacity(160);
        content->addChild(empty);
        content->updateLayout();
        m_scroll->scrollToTop();
        return;
    }

    Ref<LevelBrowserLayer> self(this);
    std::string const      destKey = levelKeyFor(m_editor->m_level);

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
        row->setContentSize({rowWidth, kRowHeight});
        row->setAnchorPoint({0.f, 0.f});
        row->setLayout(AnchorLayout::create());

        auto bg = CCLayerColor::create({0, 0, 0, 60}, rowWidth, kRowHeight);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({.5f, .5f});
        row->addChildAtPosition(bg, Anchor::Center);

        auto keyLbl = CCLabelBMFont::create(shorten(lv.levelKey, 32).c_str(), "chatFont.fnt");
        keyLbl->setScale(.55f);
        keyLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(keyLbl, Anchor::Left, {6.f, 10.f});

        std::string sub = std::to_string(lv.commitCount) + " commits - "
            + formatTimestamp(lv.lastCreatedAt);
        auto subLbl = CCLabelBMFont::create(sub.c_str(), "chatFont.fnt");
        subLbl->setScale(.45f);
        subLbl->setOpacity(200);
        subLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(subLbl, Anchor::Left, {6.f, -10.f});

        auto menu = CCMenu::create();
        menu->setContentSize({kRowMenuWidth, kRowHeight});
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
                        if (!self || !self->m_editor) {
                            if (self) finishBusyAction(self->m_busy);
                            return;
                        }
                        Ref<LevelEditorLayer> editorRef(self->m_editor);
                        Ref<EditorPauseLayer> pauseRef(self->m_pauseLayer);
                        Ref<LevelBrowserLayer> alive(self.data());
                        postToGitWorker([alive, editorRef, pauseRef, levelKey, destKey]() {
                            auto outcome = sharedGitService().importLevelFrom(destKey, levelKey);
                            geode::queueInMainThread([alive, editorRef, pauseRef, outcome = std::move(outcome)]() mutable {
                                if (!alive) return;
                                finishBusyAction(alive->m_busy);
                                auto* editor = editorRef.data();
                                auto* pause = pauseRef.data();
                                if (!outcome.ok) {
                                    Notification::create(
                                        ("Load failed: " + outcome.error).c_str(),
                                        NotificationIcon::Error
                                    )->show();
                                    return;
                                }
                                if (!canApplyEditorResult(editor)) {
                                    Notification::create(
                                        "Load succeeded but editor is no longer active",
                                        NotificationIcon::Warning
                                    )->show();
                                    return;
                                }
                                if (!applyLevelState(editor, outcome.state)) {
                                    Notification::create(
                                        "Load saved to the mod, but the editor would not apply the level",
                                        NotificationIcon::Warning
                                    )->show();
                                } else {
                                    Notification::create("Level loaded", NotificationIcon::Success)->show();
                                }
                                alive->onClose(nullptr);
                                if (pause) {
                                    pause->onResume(nullptr);
                                }
                            });
                        });
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
                    postToGitWorker([alive, levelKey]() {
                        bool const ok = sharedCommitStore().deleteLevel(levelKey);
                        geode::queueInMainThread([alive, ok]() {
                            if (!alive) return;
                            finishBusyAction(alive->m_busy);
                            if (!ok) {
                                Notification::create("Delete failed", NotificationIcon::Error)->show();
                                return;
                            }
                            Notification::create("Level history removed", NotificationIcon::Success)
                                ->show();
                            alive->rebuildList();
                        });
                    });
                }
            );
        });
        menu->addChild(delBtn);
        menu->updateLayout();
        row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});

        content->addChild(row);
    }

    content->updateLayout();
    m_scroll->scrollToTop();
}

} // namespace git_editor
