#include "../editor/LevelStateIO.hpp"
#include "../service/GitService.hpp"
#include "../ui/CommitMessageLayer.hpp"
#include "../ui/HistoryLayer.hpp"
#include "../ui/LevelBrowserLayer.hpp"
#include "../ui/common/GitUiActionRunner.hpp"
#include "../util/GitWorker.hpp"
#include "../editor/LevelKey.hpp"
#include "../util/io/PathUtf8.hpp"
#include "../ui/presentation/UiText.hpp"

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

#include <fmt/format.h>

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
            auto name = escapePopupText(git_editor::shorten(git_editor::pathUtf8(p.filename()), 48));
            body += "- ";
            body += name;
            body += "\n";
        }
    };
    addBucket("Smart merge (shared root)", plan.smart);
    addBucket("Sequential fallback (different root)", plan.sequential);
    if (!plan.invalid.empty()) {
        if (!body.empty()) body += "\n\n";
        body += "Skipped (unreadable):\n";
        for (auto const& inv : plan.invalid) {
            auto name = escapePopupText(git_editor::shorten(git_editor::pathUtf8(inv.path.filename()), 40));
            auto reason = escapePopupText(git_editor::shorten(inv.reason, 60));
            body += "- ";
            body += name;
            body += ": ";
            body += reason;
            body += "\n";
        }
    }
    if (!body.empty()) body += "\nProceed?";
    return body;
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

void notifyImportMergeOutcome(
    git_editor::Result<void> const& fin,
    git_editor::ImportManyPayload const& payload
) {
    if (!fin.ok) {
        Notification::create(
            ("Editor merged but DB write failed: " + fin.error).c_str(),
            NotificationIcon::Error
        )->show();
        return;
    }
    Notification::create(
        fmt::format(
            "Merged {} smart + {} sequential, conflicts {}, skipped {}",
            payload.smartCount,
            payload.sequentialCount,
            payload.conflictCount,
            payload.skippedCount
        ).c_str(),
        NotificationIcon::Success
    )->show();
}

bool tryApplyImportMerge(LevelEditorLayer* editor, git_editor::LevelState const& state) {
    if (!editor->getParent()) {
        Notification::create(
            "Merge ready but editor is no longer active, aborted before DB write",
            NotificationIcon::Warning
        )->show();
        return false;
    }
    if (!git_editor::applyLevelState(editor, state)) {
        Notification::create(
            "Editor refused merge, aborted before DB write",
            NotificationIcon::Warning
        )->show();
        return false;
    }
    return true;
}

void runImportMergeFinalize(git_editor::PendingMergeImport pending, git_editor::ImportManyPayload payload) {
    git_editor::ui_action_runner::runWorkerResult<git_editor::Result<void>>(
        [pending = std::move(pending), payload]() mutable {
            return git_editor::sharedGitService().finalizeImportManyFromGdge(pending, payload.state);
        },
        [payload](git_editor::Result<void> fin) {
            notifyImportMergeOutcome(fin, payload);
        }
    );
}

void runImportMergePrepare(
    Ref<EditorPauseLayer> alive,
    Ref<LevelEditorLayer> editorRef,
    std::string levelKey,
    std::vector<std::filesystem::path> paths
) {
    git_editor::ui_action_runner::runWorkerResult<git_editor::Prepared<git_editor::ImportManyPayload>>(
        [levelKey, paths = std::move(paths)]() mutable {
            return git_editor::sharedGitService().prepareImportManyFromGdge(levelKey, paths);
        },
        [alive, editorRef](git_editor::Prepared<git_editor::ImportManyPayload> prep) mutable {
            auto* editorPtr = editorRef.data();
            if (!alive || !editorPtr) return;
            if (!prep.result.ok) {
                Notification::create(
                    ("Multi-merge failed: " + prep.result.error).c_str(),
                    NotificationIcon::Error
                )->show();
                return;
            }
            if (!tryApplyImportMerge(editorPtr, prep.result.value.state)) return;
            if (!prep.pendingMergeImport || prep.pendingMergeImport->commits.empty()) {
                return;
            }
            auto payload = prep.result.value;
            auto pending = std::move(*prep.pendingMergeImport);
            runImportMergeFinalize(std::move(pending), payload);
        }
    );
}

void showImportPlanPopup(
    Ref<EditorPauseLayer> alive,
    Ref<LevelEditorLayer> editorRef,
    std::string levelKey,
    std::vector<std::filesystem::path> paths,
    git_editor::ImportPlan plan
) {
    auto* editorPtr = editorRef.data();
    if (!alive || !editorPtr) return;
    if (plan.smart.empty() && plan.sequential.empty()) {
        std::string msg = "No valid .gdge files selected";
        if (!plan.invalid.empty()) {
            auto const& first = plan.invalid.front();
            auto name = git_editor::shorten(git_editor::pathUtf8(first.path.filename()), 32);
            auto reason = git_editor::shorten(first.reason, 80);
            msg = "Invalid .gdge: " + name + ": " + reason;
            if (plan.invalid.size() > 1) {
                msg += " (+" + std::to_string(plan.invalid.size() - 1) + " more)";
            }
        }
        Notification::create(msg.c_str(), NotificationIcon::Error)->show();
        return;
    }
    createQuickPopup(
        "Import plan",
        planBody(plan).c_str(),
        "Cancel", "Merge",
        [alive, editorRef, levelKey, paths = std::move(paths)](FLAlertLayer*, bool yes) mutable {
            auto* editorPtr = editorRef.data();
            if (!yes || !alive || !editorPtr) return;
            runImportMergePrepare(alive, editorRef, levelKey, std::move(paths));
        }
    );
}

void startImportGdgeFlow(
    Ref<EditorPauseLayer> alive,
    Ref<LevelEditorLayer> editorRef,
    std::string levelKey,
    std::vector<std::filesystem::path> paths
) {
    auto pathsForWorker = paths;
    git_editor::ui_action_runner::runWorkerResult<git_editor::ImportPlan>(
        [levelKey, paths = std::move(pathsForWorker)]() {
            return git_editor::sharedGitService().planImport(levelKey, paths);
        },
        [alive, editorRef, levelKey, paths = std::move(paths)](git_editor::ImportPlan plan) mutable {
            showImportPlanPopup(alive, editorRef, levelKey, std::move(paths), std::move(plan));
        }
    );
}

} // namespace

class $modify(GitEditorPauseHook, EditorPauseLayer) {
    struct Fields {};

    void customSetup() {
        EditorPauseLayer::customSetup();

        Ref<EditorPauseLayer> safeSelf(this);
        // Defer one frame so other mods finish customSetup.
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
                startImportGdgeFlow(alive, editorRef, levelKey, std::move(paths));
            }
        );
    }
};
