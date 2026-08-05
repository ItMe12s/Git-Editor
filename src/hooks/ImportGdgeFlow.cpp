#include "ImportGdgeFlow.hpp"

#include "service/GitService.hpp"
#include "ui/HistoryActions.hpp"
#include "ui/common/GitUiActionRunner.hpp"
#include "util/format/Shorten.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/string.hpp>
#include <fmt/format.h>

using namespace geode::prelude;

namespace git_editor::import_gdge_flow {

    namespace {

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

        std::string planBody(ImportPlan const& plan) {
            std::string body;
            if (plan.noLocalCommits) {
                body +=
                    "WARNING: this level has no commits. Import will overwrite level state from "
                    "selected .gdge file(s).";
            }
            auto addBucket = [&](char const* title, std::vector<std::filesystem::path> const& paths) {
                if (paths.empty()) return;
                if (!body.empty()) body += "\n\n";
                body += title;
                body += ":\n";
                for (auto const& p : paths) {
                    auto name =
                        escapePopupText(shorten(geode::utils::string::pathToString(p.filename()), 48));
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
                    auto name = escapePopupText(
                        shorten(geode::utils::string::pathToString(inv.path.filename()), 40)
                    );
                    auto reason = escapePopupText(shorten(inv.reason, 60));
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

        void notifyImportMergeOutcome(Result<void> const& fin, ImportManyPayload const& payload) {
            if (!fin) {
                geode::Notification::create(
                    ("Editor merged but DB write failed: " + fin.error()).c_str(),
                    geode::NotificationIcon::Error
                )
                    ->show();
                return;
            }
            geode::Notification::create(
                fmt::format(
                    "Merged {} smart + {} sequential, conflicts {}, skipped {}",
                    payload.smartCount,
                    payload.sequentialCount,
                    payload.conflictCount,
                    payload.skippedCount
                )
                    .c_str(),
                geode::NotificationIcon::Success
            )
                ->show();
        }

        void runImportMergeFinalize(PendingMergeImport pending, ImportManyPayload payload) {
            ui_action_runner::runWorkerResult<Result<void>>(
                [pending = std::move(pending)]() mutable {
                    return sharedGitService().finalizeImportManyFromGdge(pending);
                },
                [payload = std::move(payload)](Result<void> fin) {
                    notifyImportMergeOutcome(fin, payload);
                }
            );
        }

        void runImportMergePrepare(
            geode::Ref<EditorPauseLayer> alive, geode::Ref<LevelEditorLayer> editorRef,
            std::string levelKey, std::vector<std::filesystem::path> paths
        ) {
            ui_action_runner::runWorkerResult<Prepared<ImportManyPayload>>(
                [levelKey, paths = std::move(paths)]() mutable {
                    return sharedGitService().prepareImportManyFromGdge(levelKey, paths);
                },
                [alive, editorRef](Prepared<ImportManyPayload> prep) mutable {
                    auto* editorPtr = editorRef.data();
                    if (!alive || !editorPtr) return;
                    if (!prep.result) {
                        geode::Notification::create(
                            ("Multi-merge failed: " + prep.result.error()).c_str(),
                            geode::NotificationIcon::Error
                        )
                            ->show();
                        return;
                    }
                    if (!history_actions::applyStateToEditorOrNotify(
                            "Merge", editorPtr, prep.result->state, prep.result->conflictCount > 0
                        ))
                        return;
                    if (!prep.pendingMergeImport || prep.pendingMergeImport->commits.empty()) {
                        return;
                    }
                    auto payload = std::move(*prep.result);
                    auto pending = std::move(*prep.pendingMergeImport);
                    runImportMergeFinalize(std::move(pending), std::move(payload));
                }
            );
        }

        void showImportPlanPopup(
            geode::Ref<EditorPauseLayer> alive, geode::Ref<LevelEditorLayer> editorRef,
            std::string levelKey, std::vector<std::filesystem::path> paths, ImportPlan plan
        ) {
            auto* editorPtr = editorRef.data();
            if (!alive || !editorPtr) return;
            if (plan.smart.empty() && plan.sequential.empty()) {
                std::string msg = "No valid .gdge files selected";
                if (!plan.invalid.empty()) {
                    auto const& first = plan.invalid.front();
                    auto name =
                        shorten(geode::utils::string::pathToString(first.path.filename()), 32);
                    auto reason = shorten(first.reason, 80);
                    msg = "Invalid .gdge: " + name + ": " + reason;
                    if (plan.invalid.size() > 1) {
                        msg += " (+" + std::to_string(plan.invalid.size() - 1) + " more)";
                    }
                }
                geode::Notification::create(msg.c_str(), geode::NotificationIcon::Error)->show();
                return;
            }
            createQuickPopup(
                "Import plan",
                planBody(plan).c_str(),
                "Cancel",
                "Merge",
                [alive, editorRef, levelKey, paths = std::move(paths)](FLAlertLayer*, bool yes) mutable {
                    auto* editorPtr = editorRef.data();
                    if (!yes || !alive || !editorPtr) return;
                    runImportMergePrepare(alive, editorRef, levelKey, std::move(paths));
                }
            );
        }

    } // namespace

    void startImportGdgeFlow(
        geode::Ref<EditorPauseLayer> alive, geode::Ref<LevelEditorLayer> editorRef,
        std::string levelKey, std::vector<std::filesystem::path> paths
    ) {
        auto pathsForWorker = paths;
        ui_action_runner::runWorkerResult<ImportPlan>(
            [levelKey, paths = std::move(pathsForWorker)]() {
                return sharedGitService().planImport(levelKey, paths);
            },
            [alive, editorRef, levelKey, paths = std::move(paths)](ImportPlan plan) mutable {
                showImportPlanPopup(alive, editorRef, levelKey, std::move(paths), std::move(plan));
            }
        );
    }

} // namespace git_editor::import_gdge_flow
