#include "ImportGdgeFlow.hpp"
#include "editor/LevelKey.hpp"
#include "editor/LevelStateIO.hpp"
#include "service/GitService.hpp"
#include "ui/CommitMessageLayer.hpp"
#include "ui/HistoryLayer.hpp"
#include "ui/LevelBrowserLayer.hpp"
#include "ui/common/GitUiActionRunner.hpp"
#include "util/GitWorker.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <optional>

using namespace geode::prelude;

namespace {

constexpr auto kTopMenuID = "top-menu"_spr;

std::string currentLevelKey(LevelEditorLayer* editor) {
    return editor ? git_editor::levelKeyFor(editor->m_level) : "invalid:no-editor";
}

bool isInvalidLevelKey(std::string const& levelKey) {
    return levelKey.rfind("invalid:", 0) == 0;
}

bool requireActiveEditor(LevelEditorLayer* editor) {
    if (editor) return true;
    Notification::create("No active editor", NotificationIcon::Error)->show();
    return false;
}

std::optional<std::string> requireValidLevelKey(LevelEditorLayer* editor) {
    if (!requireActiveEditor(editor)) return std::nullopt;
    auto levelKey = currentLevelKey(editor);
    if (!isInvalidLevelKey(levelKey)) return levelKey;
    Notification::create("No valid level context", NotificationIcon::Error)->show();
    return std::nullopt;
}

void notifyCommitOutcome(git_editor::Result<git_editor::CommitId> const& outcome) {
    if (outcome.ok) {
        Notification::create("Committed", NotificationIcon::Success)->show();
    } else {
        Notification::create(
            ("Commit failed: " + outcome.error).c_str(),
            NotificationIcon::Error
        )->show();
    }
}

void notifyExportOutcome(git_editor::Result<void> const& outcome) {
    if (!outcome.ok) {
        Notification::create(
            ("Export failed: " + outcome.error).c_str(),
            NotificationIcon::Error
        )->show();
        return;
    }
    Notification::create("Exported .gdge", NotificationIcon::Success)->show();
}

} // namespace

class $modify(GitEditorPauseHook, EditorPauseLayer) {
    struct Fields {};

    void customSetup() {
        EditorPauseLayer::customSetup();

        Ref<EditorPauseLayer> safeSelf(this);
        // Defer one frame so other mods fully finish first like BetterEdit.
        // Honestly, I wouldn't use hook priority.
        geode::queueInMainThread([safeSelf]() {
            auto* self = safeSelf.data();
            if (!self || !self->getParent()) return;
            static_cast<GitEditorPauseHook*>(self)->installTopMenu();
        });
    }

    void installTopMenu() {
        if (this->getChildByID(kTopMenuID)) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto const sizeMultiplier = std::clamp(
            static_cast<float>(Mod::get()->getSettingValue<double>("size-multiplier")),
            0.2f,
            2.0f
        );

        auto menu = CCMenu::create();
        menu->setID(kTopMenuID);
        menu->setContentSize({360.f, 56.f});
        menu->setPosition({ winSize.width / 2.f, winSize.height - 45.f });
        menu->setLayout(
            ColumnLayout::create()
                ->setAxisReverse(true)
                ->setGap(4.f * sizeMultiplier)
                ->setAxisAlignment(AxisAlignment::Center)
                ->setAutoGrowAxis(0.f)
                ->setCrossAxisOverflow(true)
        );

        auto makeRow = [&](char const* id) {
            auto row = CCMenu::create();
            row->setID(id);
            row->setContentSize({360.f, 26.f});
            row->setLayout(
                RowLayout::create()
                    ->setGap(6.f * sizeMultiplier)
                    ->setAxisAlignment(AxisAlignment::Center)
                    ->setAutoGrowAxis(0.f)
                    ->setCrossAxisOverflow(true)
            );
            return row;
        };

        auto rowTop = makeRow("top-row"_spr);
        auto rowBottom = makeRow("bottom-row"_spr);

        auto commitSpr = ButtonSprite::create("Commit", "bigFont.fnt", "GJ_button_01.png", .8f);
        commitSpr->setScale(.5f * sizeMultiplier);
        auto commitBtn = CCMenuItemExt::createSpriteExtra(commitSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitCommit();
        });
        commitBtn->setID("commit-button"_spr);
        rowTop->addChild(commitBtn);

        auto historySpr = ButtonSprite::create("History", "bigFont.fnt", "GJ_button_04.png", .8f);
        historySpr->setScale(.5f * sizeMultiplier);
        auto historyBtn = CCMenuItemExt::createSpriteExtra(historySpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitHistory();
        });
        historyBtn->setID("history-button"_spr);
        rowTop->addChild(historyBtn);

        auto levelsSpr = ButtonSprite::create("Levels", "bigFont.fnt", "GJ_button_05.png", .8f);
        levelsSpr->setScale(.5f * sizeMultiplier);
        auto levelsBtn = CCMenuItemExt::createSpriteExtra(levelsSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitLevels();
        });
        levelsBtn->setID("levels-button"_spr);
        rowTop->addChild(levelsBtn);

        auto importSpr = ButtonSprite::create("Import .gdge", "bigFont.fnt", "GJ_button_02.png", .7f);
        importSpr->setScale(.5f * sizeMultiplier);
        auto importBtn = CCMenuItemExt::createSpriteExtra(importSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitImportGdge();
        });
        importBtn->setID("import-gdge-button"_spr);
        rowBottom->addChild(importBtn);

        auto exportSpr = ButtonSprite::create("Export .gdge", "bigFont.fnt", "GJ_button_06.png", .7f);
        exportSpr->setScale(.5f * sizeMultiplier);
        auto exportBtn = CCMenuItemExt::createSpriteExtra(exportSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitExportGdge();
        });
        exportBtn->setID("export-gdge-button"_spr);
        rowBottom->addChild(exportBtn);

        rowTop->updateLayout();
        rowBottom->updateLayout();
        menu->addChild(rowTop);
        menu->addChild(rowBottom);
        menu->updateLayout();
        this->addChild(menu);
    }

    void onGitCommit() {
        auto editor = m_editorLayer;
        if (!requireActiveEditor(editor)) return;
        Ref<LevelEditorLayer> editorRef(editor);

        auto popup = git_editor::CommitMessageLayer::create(
            [editorRef](std::string const& message) {
                auto* editorPtr = editorRef.data();
                auto levelKey = requireValidLevelKey(editorPtr);
                if (!levelKey) return;
                auto levelStr = git_editor::captureLevelString(editorPtr);
                if (levelStr.empty()) {
                    Notification::create(
                        "Level is empty", NotificationIcon::Warning
                    )->show();
                    return;
                }
                git_editor::ui_action_runner::runWorkerResult<git_editor::Result<git_editor::CommitId>>(
                    [levelKey = *levelKey, message, levelStr]() {
                        return git_editor::sharedGitService().commit(levelKey, message, levelStr);
                    },
                    [](git_editor::Result<git_editor::CommitId> outcome) {
                        notifyCommitOutcome(outcome);
                    }
                );
            }
        );
        if (popup) popup->show();
    }

    void onGitHistory() {
        auto editor = m_editorLayer;
        auto levelKey = requireValidLevelKey(editor);
        if (!levelKey) return;
        Ref<LevelEditorLayer> editorRef(editor);
        Ref<EditorPauseLayer> pauseRef(this);
        if (auto popup = git_editor::HistoryLayer::create(*levelKey, editorRef.data(), pauseRef.data())) {
            popup->show();
        }
    }

    void onGitLevels() {
        auto* editor = m_editorLayer;
        if (!requireActiveEditor(editor)) return;
        Ref<LevelEditorLayer> editorRef(editor);
        Ref<EditorPauseLayer> pauseRef(this);
        if (auto popup = git_editor::LevelBrowserLayer::create(editorRef.data(), pauseRef.data())) {
            popup->show();
        }
    }

    void onGitExportGdge() {
        auto* editor = m_editorLayer;
        auto levelKey = requireValidLevelKey(editor);
        if (!levelKey) return;

        geode::utils::file::FilePickOptions options;
        options.defaultPath = geode::Mod::get()->getSaveDir() / "level-export.gdge";
        options.filters.push_back({ "Git Editor Level Package", { "*.gdge" } });
        geode::async::spawn(
            geode::utils::file::pick(geode::utils::file::PickMode::SaveFile, options),
            [levelKey = *levelKey](geode::utils::file::PickResult picked) {
                if (picked.isErr()) {
                    Notification::create("Export picker failed", NotificationIcon::Error)->show();
                    return;
                }
                auto pickedPath = picked.unwrap();
                if (!pickedPath) return;
                auto path = *pickedPath;
                git_editor::ui_action_runner::runWorkerResult<git_editor::Result<void>>(
                    [levelKey, path]() {
                        return git_editor::sharedGitService().exportLevelToGdge(levelKey, path);
                    },
                    [](git_editor::Result<void> outcome) {
                        notifyExportOutcome(outcome);
                    }
                );
            }
        );
    }

    void onGitImportGdge() {
        auto* editor = m_editorLayer;
        auto levelKey = requireValidLevelKey(editor);
        if (!levelKey) return;

        geode::utils::file::FilePickOptions options;
        options.defaultPath = geode::Mod::get()->getSaveDir();
        options.filters.push_back({ "Git Editor Level Package", { "*.gdge" } });
        Ref<EditorPauseLayer> alive(this);
        Ref<LevelEditorLayer> editorRef(editor);

        geode::async::spawn(
            geode::utils::file::pickMany(options),
            [alive, editorRef, levelKey = *levelKey](geode::utils::file::PickManyResult picked) {
                if (!alive) return;
                if (picked.isErr()) {
                    Notification::create("Import picker failed", NotificationIcon::Error)->show();
                    return;
                }
                auto picks = picked.unwrap();
                if (picks.empty()) return;
                std::vector<std::filesystem::path> paths;
                paths.reserve(picks.size());
                for (auto const& p : picks) paths.push_back(p);
                git_editor::import_gdge_flow::startImportGdgeFlow(
                    alive, editorRef, levelKey, std::move(paths)
                );
            }
        );
    }
};
