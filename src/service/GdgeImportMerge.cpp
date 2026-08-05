#include "GdgeImportMerge.hpp"

#include "MergeService.hpp"
#include "PackageReconstruction.hpp"
#include "core/Result.hpp"
#include "diff/Delta.hpp"
#include "diff/Differ.hpp"
#include "store/GdgePackage.hpp"
#include "util/format/Shorten.hpp"

#include <Geode/utils/string.hpp>
#include <fmt/format.h>
#include <string>
#include <utility>
#include <vector>

namespace {

    git_editor::Result<git_editor::LevelState> loadGdgeHead(std::filesystem::path const& path) {
        auto pkg = git_editor::readGdgePackage(path);
        auto const name = geode::utils::string::pathToString(path.filename());
        if (!pkg) {
            return std::unexpected(name + ": " + pkg.error());
        }
        if (pkg->commits.empty() || !pkg->metadata.headIndex) {
            return std::unexpected(name + ": missing commits or head_index");
        }
        auto head = git_editor::reconstructPackageHead(*pkg);
        if (!head) {
            return std::unexpected(name + ": package history graph invalid");
        }
        return std::move(*head);
    }

} // namespace

namespace git_editor::gdge_import_merge {

    Prepared<ImportManyPayload> prepareImportManyFromGdge(
        LevelKey const& dest, ImportPlan const& plan, std::optional<CommitId> headBefore,
        LevelState ours, LevelState rootBefore
    ) {
        Prepared<ImportManyPayload> out;
        ImportManyPayload payload;
        payload.skippedCount += static_cast<int>(plan.invalid.size());

        PendingMergeImport pendingMerge;
        LevelState runningState = std::move(ours);
        bool anyMerged = false;
        std::string lastError;

        if (!plan.smart.empty()) {
            LevelState merged = runningState;
            int conflicts = 0;
            std::vector<std::string> names;
            names.reserve(plan.smart.size());
            bool ok = true;
            std::string err;
            for (auto const& inPath : plan.smart) {
                auto loaded = loadGdgeHead(inPath);
                if (!loaded) {
                    err = loaded.error();
                    ok = false;
                    break;
                }
                int stepConflicts = 0;
                auto step = mergeStates3Way(rootBefore, merged, *loaded, stepConflicts);
                if (!step) {
                    err = "3-way merge failed";
                    ok = false;
                    break;
                }
                merged = std::move(*step);
                conflicts += stepConflicts;
                names.push_back(geode::utils::string::pathToString(inPath.filename()));
            }
            if (!ok) {
                payload.skippedCount += static_cast<int>(plan.smart.size());
                if (lastError.empty()) lastError = err;
            }
            else {
                std::string preview;
                for (std::size_t i = 0; i < names.size(); ++i) {
                    if (i > 0) preview += ", ";
                    preview += names[i];
                    if (preview.size() >= 80) break;
                }
                auto message = shorten(
                    fmt::format("Smart merge: {} imports ({})", plan.smart.size(), preview), 120
                );
                PendingHeadUpdate p;
                p.levelKey = dest;
                p.parent = headBefore;
                p.message = std::move(message);
                p.deltaBlob = dumpDelta(diff(runningState, merged));
                pendingMerge.commits.push_back(std::move(p));

                anyMerged = true;
                payload.smartCount = static_cast<int>(plan.smart.size());
                payload.mergedCount += payload.smartCount;
                payload.conflictCount += conflicts;
                runningState = std::move(merged);
                if (!headBefore) {
                    rootBefore = runningState;
                }
            }
        }

        for (auto const& path : plan.sequential) {
            auto loaded = loadGdgeHead(path);
            if (!loaded) {
                payload.skippedCount++;
                if (lastError.empty()) lastError = loaded.error();
                continue;
            }

            bool const freshRoot = !headBefore && pendingMerge.commits.empty();

            PendingHeadUpdate p;
            p.levelKey = dest;
            if (freshRoot) {
                p.message = "Import .gdge: " + geode::utils::string::pathToString(path.filename());
                p.deltaBlob = dumpDelta(diff(LevelState{}, *loaded));
                rootBefore = *loaded;
                runningState = std::move(*loaded);
            }
            else {
                int conflicts = 0;
                auto merged = mergeStates3Way(rootBefore, runningState, *loaded, conflicts);
                if (!merged) {
                    payload.skippedCount++;
                    if (lastError.empty()) {
                        lastError = geode::utils::string::pathToString(path.filename()) +
                            ": 3-way merge failed";
                    }
                    continue;
                }
                p.message = "Merge import: " + geode::utils::string::pathToString(path.filename());
                p.deltaBlob = dumpDelta(diff(runningState, *merged));
                if (!pendingMerge.commits.empty()) {
                    p.parentPendingIx = pendingMerge.commits.size() - 1;
                }
                else {
                    p.parent = headBefore;
                }
                payload.conflictCount += conflicts;
                runningState = std::move(*merged);
            }
            pendingMerge.commits.push_back(std::move(p));
            anyMerged = true;
            payload.sequentialCount++;
            payload.mergedCount++;
        }

        if (!anyMerged) {
            out.result =
                std::unexpected(lastError.empty() ? "none of selected files merged" : lastError);
            return out;
        }

        payload.state = std::move(runningState);
        if (!pendingMerge.commits.empty()) {
            pendingMerge.commits.back().cacheState = payload.state;
        }

        out.result = std::move(payload);
        out.pendingMergeImport = std::move(pendingMerge);
        return out;
    }

} // namespace git_editor::gdge_import_merge
