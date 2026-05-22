#include "GdgeImportMerge.hpp"

#include "MergeService.hpp"
#include "PackageReconstruction.hpp"

#include "core/Result.hpp"
#include "diff/Delta.hpp"
#include "diff/Differ.hpp"
#include "store/GdgePackage.hpp"
#include "util/format/Shorten.hpp"
#include "util/io/PathUtf8.hpp"

#include <fmt/format.h>

#include <string>
#include <utility>
#include <vector>

namespace {

git_editor::Result<git_editor::LevelState> loadGdgeHead(std::filesystem::path const& path) {
    git_editor::Result<git_editor::LevelState> out;
    auto pkg = git_editor::readGdgePackage(path);
    if (!pkg.ok) {
        out.error = git_editor::pathUtf8(path.filename()) + ": " + pkg.error;
        return out;
    }
    if (pkg.value.commits.empty() || !pkg.value.metadata.headIndex) {
        out.error = git_editor::pathUtf8(path.filename()) + ": missing commits or head_index";
        return out;
    }
    auto head = git_editor::reconstructPackageHead(pkg.value);
    if (!head) {
        out.error = git_editor::pathUtf8(path.filename()) + ": package history graph invalid";
        return out;
    }
    out.ok    = true;
    out.value = std::move(*head);
    return out;
}

} // namespace

namespace git_editor::gdge_import_merge {

Prepared<ImportManyPayload> prepareImportManyFromGdge(
    LevelKey const&         dest,
    ImportPlan const&       plan,
    std::optional<CommitId> headBefore,
    LevelState              ours,
    LevelState              rootBefore
) {
    Prepared<ImportManyPayload> out;
    ImportManyPayload           payload;
    payload.skippedCount += static_cast<int>(plan.invalid.size());

    PendingMergeImport pendingMerge;
    LevelState         runningState = ours;
    bool               anyMerged    = false;
    std::string        lastError;

    if (!plan.smart.empty()) {
        LevelState merged = ours;
        int        conflicts = 0;
        std::vector<std::string> names;
        names.reserve(plan.smart.size());
        bool        ok = true;
        std::string err;
        for (auto const& inPath : plan.smart) {
            auto loaded = loadGdgeHead(inPath);
            if (!loaded.ok) {
                err = loaded.error;
                ok  = false;
                break;
            }
            int stepConflicts = 0;
            auto step = mergeStates3Way(rootBefore, merged, loaded.value, stepConflicts);
            if (!step) {
                err = "3-way merge failed";
                ok  = false;
                break;
            }
            merged = std::move(*step);
            conflicts += stepConflicts;
            names.push_back(git_editor::pathUtf8(inPath.filename()));
        }
        if (!ok) {
            payload.skippedCount += static_cast<int>(plan.smart.size());
            if (lastError.empty()) lastError = err;
        } else {
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
            p.levelKey  = dest;
            p.parent    = headBefore;
            p.message   = std::move(message);
            p.deltaBlob = dumpDelta(diff(ours, merged));
            pendingMerge.commits.push_back(std::move(p));

            anyMerged = true;
            payload.smartCount = static_cast<int>(plan.smart.size());
            payload.mergedCount += payload.smartCount;
            payload.conflictCount += conflicts;
            runningState = std::move(merged);
            payload.state = runningState;
            if (!headBefore) {
                rootBefore = runningState;
            }
        }
    }

    for (auto const& path : plan.sequential) {
        auto loaded = loadGdgeHead(path);
        if (!loaded.ok) {
            payload.skippedCount++;
            if (lastError.empty()) lastError = loaded.error;
            continue;
        }

        bool const freshRoot = !headBefore && pendingMerge.commits.empty();

        PendingHeadUpdate p;
        p.levelKey = dest;
        if (freshRoot) {
            p.message    = "Import .gdge: " + git_editor::pathUtf8(path.filename());
            p.deltaBlob  = dumpDelta(diff(LevelState {}, loaded.value));
            runningState = loaded.value;
            rootBefore   = loaded.value;
        } else {
            int conflicts = 0;
            auto merged = mergeStates3Way(rootBefore, runningState, loaded.value, conflicts);
            if (!merged) {
                payload.skippedCount++;
                if (lastError.empty()) {
                    lastError = git_editor::pathUtf8(path.filename()) + ": 3-way merge failed";
                }
                continue;
            }
            p.message   = "Merge import: " + git_editor::pathUtf8(path.filename());
            p.deltaBlob = dumpDelta(diff(runningState, *merged));
            if (!pendingMerge.commits.empty()) {
                p.parentPendingIx = pendingMerge.commits.size() - 1;
            } else {
                p.parent = headBefore;
            }
            payload.conflictCount += conflicts;
            runningState = std::move(*merged);
        }
        pendingMerge.commits.push_back(std::move(p));
        anyMerged = true;
        payload.sequentialCount++;
        payload.mergedCount++;
        payload.state = runningState;
    }

    if (!anyMerged) {
        out.result.error = lastError.empty() ? "none of selected files merged" : lastError;
        return out;
    }

    if (!pendingMerge.commits.empty()) {
        pendingMerge.commits.back().cacheState = payload.state;
    }

    out.result.ok          = true;
    out.result.value       = std::move(payload);
    out.pendingMergeImport = std::move(pendingMerge);
    return out;
}

} // namespace git_editor::gdge_import_merge
