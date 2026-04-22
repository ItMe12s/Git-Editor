#include "../editor/LevelStateIO.hpp"
#include "../service/GitService.hpp"
#include "../ui/CommitMessageLayer.hpp"
#include "../ui/HistoryLayer.hpp"
#include "../ui/LevelBrowserLayer.hpp"
#include "../util/AsyncQueue.hpp"
#include "../util/LevelKey.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>

using namespace geode::prelude;

namespace {

// "_spr" prefixes mod id (avoids ID collisions).
constexpr auto kTopMenuID = "top-menu"_spr;

std::string currentLevelKey(LevelEditorLayer* editor) {
    return editor ? git_editor::levelKeyFor(editor->m_level) : "invalid:no-editor";
}

bool isInvalidLevelKey(std::string const& levelKey) {
    return levelKey.rfind("invalid:", 0) == 0;
}

} // namespace

class $modify(GitEditorPauseHook, EditorPauseLayer) {
    struct Fields {};

    void customSetup() {
        EditorPauseLayer::customSetup();

        Ref<EditorPauseLayer> safeSelf(this);
        // Defer one tick so other mods finish customSetup before we attach the menu.
        geode::queueInMainThread([safeSelf]() {
            auto* self = safeSelf.data();
            if (!self || !self->getParent()) return;
            static_cast<GitEditorPauseHook*>(self)->installTopMenu();
        });
    }

    void installTopMenu() {
        if (this->getChildByID(kTopMenuID)) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setID(kTopMenuID);
        menu->setContentSize({220.f, 26.f});
        menu->setPosition({ winSize.width / 2.f, winSize.height - 14.f });
        menu->setLayout(
            RowLayout::create()
                ->setGap(6.f)
                ->setAxisAlignment(AxisAlignment::Center)
                ->setAutoGrowAxis(0.f)
                ->setCrossAxisOverflow(true)
        );

        auto commitSpr = ButtonSprite::create("Commit", "bigFont.fnt", "GJ_button_01.png", .8f);
        commitSpr->setScale(.5f);
        auto commitBtn = CCMenuItemExt::createSpriteExtra(commitSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitCommit();
        });
        commitBtn->setID("commit-button"_spr);
        menu->addChild(commitBtn);

        auto historySpr = ButtonSprite::create("History", "bigFont.fnt", "GJ_button_04.png", .8f);
        historySpr->setScale(.5f);
        auto historyBtn = CCMenuItemExt::createSpriteExtra(historySpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitHistory();
        });
        historyBtn->setID("history-button"_spr);
        menu->addChild(historyBtn);

        auto levelsSpr = ButtonSprite::create("Levels", "bigFont.fnt", "GJ_button_05.png", .8f);
        levelsSpr->setScale(.5f);
        auto levelsBtn = CCMenuItemExt::createSpriteExtra(levelsSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitLevels();
        });
        levelsBtn->setID("levels-button"_spr);
        menu->addChild(levelsBtn);

        menu->updateLayout();
        this->addChild(menu);
    }

    void onGitCommit() {
        auto editor = m_editorLayer;
        if (!editor) {
            Notification::create("No active editor", NotificationIcon::Error)->show();
            return;
        }

        auto popup = git_editor::CommitMessageLayer::create(
            [editor](std::string const& message) {
                auto levelKey = currentLevelKey(editor);
                if (isInvalidLevelKey(levelKey)) {
                    Notification::create("No valid level context", NotificationIcon::Error)->show();
                    return;
                }
                auto levelStr = git_editor::captureLevelString(editor);
                if (levelStr.empty()) {
                    Notification::create(
                        "Level is empty", NotificationIcon::Warning
                    )->show();
                    return;
                }
                git_editor::postToGitWorker([levelKey, message, levelStr]() {
                    auto outcome = git_editor::sharedGitService().commit(
                        levelKey, message, levelStr
                    );
                    geode::queueInMainThread([outcome = std::move(outcome)]() {
                        if (outcome.ok) {
                            Notification::create("Committed", NotificationIcon::Success)->show();
                        } else {
                            Notification::create(
                                ("Commit failed: " + outcome.error).c_str(),
                                NotificationIcon::Error
                            )->show();
                        }
                    });
                });
            }
        );
        if (popup) popup->show();
    }

    void onGitHistory() {
        auto editor = m_editorLayer;
        if (!editor) {
            Notification::create("No active editor", NotificationIcon::Error)->show();
            return;
        }
        auto levelKey = currentLevelKey(editor);
        if (isInvalidLevelKey(levelKey)) {
            Notification::create("No valid level context", NotificationIcon::Error)->show();
            return;
        }
        if (auto popup = git_editor::HistoryLayer::create(levelKey, editor, this)) {
            popup->show();
        }
    }

    void onGitLevels() {
        auto* editor = m_editorLayer;
        if (!editor) {
            Notification::create("No active editor", NotificationIcon::Error)->show();
            return;
        }
        if (auto popup = git_editor::LevelBrowserLayer::create(editor, this)) {
            popup->show();
        }
    }
};
