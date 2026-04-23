#include "../editor/LevelStateIO.hpp"
#include "../service/GitService.hpp"
#include "../ui/CommitMessageLayer.hpp"
#include "../ui/HistoryLayer.hpp"
#include "../ui/LevelBrowserLayer.hpp"
#include "../util/GitWorker.hpp"
#include "../util/LevelKey.hpp"
#include "../util/UiText.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
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

// "_spr" prefixes mod id (avoids ID collisions).
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

std::string escapePopupText(std::string s) {
    auto replaceAll = [](std::string& text, std::string const& from, std::string const& to) {
        std::size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replaceAll(s, "&", "&amp;");
    replaceAll(s, "<", "&lt;");
    replaceAll(s, ">", "&gt;");
    replaceAll(s, "\"", "&quot;");
    replaceAll(s, "'", "&#39;");
    return s;
}

std::string pathUtf8(std::filesystem::path const& path) {
    auto const u8 = path.u8string();
    return std::string(reinterpret_cast<char const*>(u8.c_str()), u8.size());
}

std::string planBody(git_editor::ImportPlan const& plan) {
    std::string body;
    if (plan.noLocalCommits) {
        body += "WARNING: this level has no commits. Import will overwrite level state from selected .gdge file(s).";
    }
    auto addBucket = [&](char const* title, std::vector<std::filesystem::path> const& paths) {
        if (paths.empty()) return;
        if (!body.empty()) body += "\n\n";
        body += title;
        body += ":\n";
        for (auto const& p : paths) {
            auto name = escapePopupText(git_editor::shorten(pathUtf8(p.filename()), 48));
            body += "- ";
            body += name;
            body += "\n";
        }
    };
    addBucket("Smart merge (shared root)", plan.smart);
    addBucket("Sequential fallback (different root)", plan.sequential);
    addBucket("Skipped (unreadable)", plan.invalid);
    if (!body.empty()) body += "\nProceed?";
    return body;
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
        auto const sizeMultiplier = std::clamp(
            static_cast<float>(Mod::get()->getSettingValue<double>("size-multiplier")),
            0.2f,
            2.0f
        );

        auto menu = CCMenu::create();
        menu->setID(kTopMenuID);
        menu->setContentSize({360.f, 26.f});
        menu->setPosition({ winSize.width / 2.f, winSize.height - 14.f });
        menu->setLayout(
            RowLayout::create()
                ->setGap(6.f * sizeMultiplier)
                ->setAxisAlignment(AxisAlignment::Center)
                ->setAutoGrowAxis(0.f)
                ->setCrossAxisOverflow(true)
        );

        auto commitSpr = ButtonSprite::create("Commit", "bigFont.fnt", "GJ_button_01.png", .8f);
        commitSpr->setScale(.5f * sizeMultiplier);
        auto commitBtn = CCMenuItemExt::createSpriteExtra(commitSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitCommit();
        });
        commitBtn->setID("commit-button"_spr);
        menu->addChild(commitBtn);

        auto historySpr = ButtonSprite::create("History", "bigFont.fnt", "GJ_button_04.png", .8f);
        historySpr->setScale(.5f * sizeMultiplier);
        auto historyBtn = CCMenuItemExt::createSpriteExtra(historySpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitHistory();
        });
        historyBtn->setID("history-button"_spr);
        menu->addChild(historyBtn);

        auto levelsSpr = ButtonSprite::create("Levels", "bigFont.fnt", "GJ_button_05.png", .8f);
        levelsSpr->setScale(.5f * sizeMultiplier);
        auto levelsBtn = CCMenuItemExt::createSpriteExtra(levelsSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitLevels();
        });
        levelsBtn->setID("levels-button"_spr);
        menu->addChild(levelsBtn);

        auto exportSpr = ButtonSprite::create("Export .gdge", "bigFont.fnt", "GJ_button_06.png", .7f);
        exportSpr->setScale(.5f * sizeMultiplier);
        auto exportBtn = CCMenuItemExt::createSpriteExtra(exportSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitExportGdge();
        });
        exportBtn->setID("export-gdge-button"_spr);
        menu->addChild(exportBtn);

        auto importSpr = ButtonSprite::create("Import .gdge", "bigFont.fnt", "GJ_button_02.png", .7f);
        importSpr->setScale(.5f * sizeMultiplier);
        auto importBtn = CCMenuItemExt::createSpriteExtra(importSpr, [this](CCMenuItemSpriteExtra*) {
            this->onGitImportGdge();
        });
        importBtn->setID("import-gdge-button"_spr);
        menu->addChild(importBtn);

        menu->updateLayout();
        this->addChild(menu);
    }

    void onGitCommit() {
        auto editor = m_editorLayer;
        if (!requireActiveEditor(editor)) return;

        auto popup = git_editor::CommitMessageLayer::create(
            [editor](std::string const& message) {
                auto levelKey = requireValidLevelKey(editor);
                if (!levelKey) return;
                auto levelStr = git_editor::captureLevelString(editor);
                if (levelStr.empty()) {
                    Notification::create(
                        "Level is empty", NotificationIcon::Warning
                    )->show();
                    return;
                }
                git_editor::postToGitWorker([levelKey = *levelKey, message, levelStr]() {
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
        auto levelKey = requireValidLevelKey(editor);
        if (!levelKey) return;
        if (auto popup = git_editor::HistoryLayer::create(*levelKey, editor, this)) {
            popup->show();
        }
    }

    void onGitLevels() {
        auto* editor = m_editorLayer;
        if (!requireActiveEditor(editor)) return;
        if (auto popup = git_editor::LevelBrowserLayer::create(editor, this)) {
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
                git_editor::postToGitWorker([levelKey, path]() {
                    auto outcome = git_editor::sharedGitService().exportLevelToGdge(levelKey, path);
                    geode::queueInMainThread([outcome = std::move(outcome)]() {
                        if (!outcome.ok) {
                            Notification::create(
                                ("Export failed: " + outcome.error).c_str(),
                                NotificationIcon::Error
                            )->show();
                            return;
                        }
                        Notification::create("Exported .gdge", NotificationIcon::Success)->show();
                    });
                });
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
        Ref<GitEditorPauseHook> alive(this);
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
                git_editor::postToGitWorker([alive, editorRef, levelKey, paths = std::move(paths)]() mutable {
                    auto plan = git_editor::sharedGitService().planImport(levelKey, paths);
                    geode::queueInMainThread(
                        [alive, editorRef, levelKey, paths = std::move(paths), plan = std::move(plan)]() mutable {
                            auto* editorPtr = editorRef.data();
                            if (!alive || !editorPtr) return;
                            if (plan.smart.empty() && plan.sequential.empty()) {
                                Notification::create(
                                    "No valid .gdge files selected",
                                    NotificationIcon::Error
                                )->show();
                                return;
                            }
                            createQuickPopup(
                                "Import plan",
                                planBody(plan).c_str(),
                                "Cancel", "Merge",
                                [alive, editorRef, levelKey, paths = std::move(paths)](FLAlertLayer*, bool yes) mutable {
                                    auto* editorPtr = editorRef.data();
                                    if (!yes || !alive || !editorPtr) return;
                                    git_editor::postToGitWorker(
                                        [alive, editorRef, levelKey, paths = std::move(paths)]() mutable {
                                            auto outcome = git_editor::sharedGitService().importManyFromGdge(levelKey, paths);
                                            geode::queueInMainThread([alive, editorRef, outcome = std::move(outcome)]() mutable {
                                                auto* editorPtr = editorRef.data();
                                                if (!alive || !editorPtr) return;
                                                if (!outcome.ok) {
                                                    Notification::create(
                                                        ("Multi-merge failed: " + outcome.error).c_str(),
                                                        NotificationIcon::Error
                                                    )->show();
                                                    return;
                                                }
                                                if (!editorPtr || !editorPtr->getParent()) {
                                                    Notification::create(
                                                        "Merge succeeded but editor is gone",
                                                        NotificationIcon::Warning
                                                    )->show();
                                                    return;
                                                }
                                                if (!git_editor::applyLevelState(editorPtr, outcome.state)) {
                                                    Notification::create(
                                                        "Merge saved but editor apply failed",
                                                        NotificationIcon::Warning
                                                    )->show();
                                                    return;
                                                }
                                                Notification::create(
                                                    ("Merged " + std::to_string(outcome.smartCount)
                                                     + " smart + " + std::to_string(outcome.sequentialCount)
                                                     + " sequential, conflicts " + std::to_string(outcome.conflictCount)
                                                     + ", skipped " + std::to_string(outcome.skippedCount)).c_str(),
                                                    NotificationIcon::Success
                                                )->show();
                                            });
                                        }
                                    );
                                }
                            );
                        }
                    );
                });
            }
        );
    }
};
