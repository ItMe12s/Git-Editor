#include "../editor/LevelStateIO.hpp"
#include "../store/CommitStore.hpp"
#include "../ui/CommitMessageLayer.hpp"
#include "../ui/HistoryLayer.hpp"
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
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>

using namespace geode::prelude;

namespace {

// String ID for our side menu. `_spr` auto-prefixes with the mod id, so the
// final node id is "imes.git-editor/side-menu" - guaranteed not to collide
// with vanilla or other mods' IDs.
constexpr auto kSideMenuID = "side-menu"_spr;

git_editor::CommitStore& store() { return git_editor::sharedCommitStore(); }

std::string currentLevelKey(LevelEditorLayer* editor) {
    if (!editor || !editor->m_level) return "unknown:0";
    return git_editor::levelKeyFor(editor->m_level);
}

} // namespace

class $modify(GitEditorPauseHook, EditorPauseLayer) {
    struct Fields {};

    void customSetup() {
        EditorPauseLayer::customSetup();

        // Defer the menu injection to the next main-thread tick. This lets
        // every other mod's customSetup / post-init work (including NodeIDs
        // restructuring and BetterEdit's layout changes) finish BEFORE we
        // perturb the child list. Without this, our extra CCMenu sibling
        // races BetterEdit's index-based child iteration and crashes it.
        //
        // We capture a weak Ref so if the user dismisses the pause layer
        // before the queued job runs, we just no-op instead of accessing a
        // dangling `this`.
        Ref<EditorPauseLayer> safeSelf(this);
        geode::queueInMainThread([safeSelf]() {
            auto* self = safeSelf.data();
            // Bail if the user already dismissed the pause layer between
            // customSetup and this deferred tick.
            if (!self || !self->getParent()) return;
            static_cast<GitEditorPauseHook*>(self)->installSideMenu();
        });
    }

    void installSideMenu() {
        // Idempotent guard: if, for any reason, this runs twice on the same
        // pause layer, don't stack menus.
        if (this->getChildByID(kSideMenuID)) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setID(kSideMenuID);
        menu->setContentSize({80.f, 100.f});
        menu->setPosition({ 30.f, winSize.height / 2.f });
        menu->setLayout(
            ColumnLayout::create()
                ->setGap(8.f)
                ->setAxisAlignment(AxisAlignment::Center)
                ->setAutoGrowAxis(0.f)
                ->setCrossAxisOverflow(true)
        );

        auto commitSpr = ButtonSprite::create("Commit", "bigFont.fnt", "GJ_button_01.png", .8f);
        commitSpr->setScale(.7f);
        auto commitBtn = CCMenuItemExt::createSpriteExtra(commitSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitCommit();
        });
        commitBtn->setID("commit-button"_spr);
        menu->addChild(commitBtn);

        auto historySpr = ButtonSprite::create("History", "bigFont.fnt", "GJ_button_04.png", .8f);
        historySpr->setScale(.7f);
        auto historyBtn = CCMenuItemExt::createSpriteExtra(historySpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitHistory();
        });
        historyBtn->setID("history-button"_spr);
        menu->addChild(historyBtn);

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
                auto levelStr = git_editor::captureLevelString(editor);
                if (levelStr.empty()) {
                    Notification::create(
                        "Level is empty", NotificationIcon::Warning
                    )->show();
                    return;
                }

                auto id = store().addCommit(levelKey, message, levelStr);
                if (id) {
                    Notification::create("Committed", NotificationIcon::Success)->show();
                } else {
                    Notification::create(
                        "Commit failed", NotificationIcon::Error
                    )->show();
                }
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
        if (auto popup = git_editor::HistoryLayer::create(levelKey, editor, this)) {
            popup->show();
        }
    }
};
