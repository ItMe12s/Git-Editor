#include "AutomatedTestHarness.hpp"

#include "model/LevelParser.hpp"
#include "util/io/PathUtf8.hpp"

#include <fmt/format.h>

namespace git_editor {

void runCollabPlanTest(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    ScopedTimer T;
    auto const basePath = testDir / "at_collab_base.gdge";
    auto const decAPath = testDir / "at_collab_a.gdge";
    auto const decBPath = testDir / "at_collab_b.gdge";
    auto const otherPath = testDir / "at_collab_other.gdge";

    R.addAction(kSuiteCollab, "deleteLevel collab keys");
    for (auto k : { kCollabBase, kCollabLay, kDecA, kDecB, kOther }) {
        st.deleteLevel(k);
    }

    if (!requireCommit(git, R, kSuiteCollab, "base_commit", T.ms(), kCollabBase, "base", levelAt(0), "commit kCollabBase base")) {
        return;
    }
    if (!requireExport(git, R, kSuiteCollab, "export_base", T.ms(), kCollabBase, basePath, fmt::format("export base {}", pathUtf8(basePath)))) {
        return;
    }
    if (!requireImportMany(git, R, kSuiteCollab, "layout_import_base", T.ms(), kCollabLay, { basePath }, "importMany kCollabLay <- base")) {
        return;
    }
    if (!requireCommit(git, R, kSuiteCollab, "layout_commit", T.ms(), kCollabLay, "lay_edit", levelAt(1), "commit kCollabLay lay_edit")) {
        return;
    }
    if (!forkExport(git, R, kSuiteCollab, T.ms(), kDecA, basePath, decAPath, 100, "a")) {
        return;
    }
    if (!forkExport(git, R, kSuiteCollab, T.ms(), kDecB, basePath, decBPath, 200, "b")) {
        return;
    }

    R.addAction(kSuiteCollab, "reset kOther");
    st.deleteLevel(kOther);
    if (!requireCommit(git, R, kSuiteCollab, "other_commit", T.ms(), kOther, "other_root", levelAt(9999), "commit kOther other_root")) {
        return;
    }
    if (!requireExport(git, R, kSuiteCollab, "export_other", T.ms(), kOther, otherPath, fmt::format("export other {}", pathUtf8(otherPath)))) {
        return;
    }

    R.addAction(kSuiteCollab, "planImport decA decB");
    auto plan = git.planImport(kCollabLay, { decAPath, decBPath });
    if (plan.smart.size() != 2) {
        R.addFail(kSuiteCollab, "smart_pair", fmt::format("smart size {}", plan.smart.size()), T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("plan A+B smart={}", plan.smart.size()));
    R.addAction(kSuiteCollab, "planImport decA other");
    auto planMix = git.planImport(kCollabLay, { decAPath, otherPath });
    if (planMix.smart.size() != 1 || planMix.sequential.size() != 1) {
        R.addFail(
            kSuiteCollab,
            "smart_vs_seq",
            fmt::format("smart {} seq {}", planMix.smart.size(), planMix.sequential.size()),
            T.ms()
        );
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("plan A+Other smart {} seq {}", planMix.smart.size(), planMix.sequential.size()));
    R.addAction(kSuiteCollab, "importMany merge two decorators");
    auto mergeTwo = git.importManyFromGdge(kCollabLay, { decAPath, decBPath });
    if (!mergeTwo.ok) {
        R.addFail(kSuiteCollab, "merge_two_decorators", mergeTwo.error, T.ms());
        return;
    }
    if (mergeTwo.value.smartCount < 2) {
        R.addFail(kSuiteCollab, "merge_smart_count", fmt::format("smartCount {}", mergeTwo.value.smartCount), T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("merge_two smartCount {}", mergeTwo.value.smartCount));

    R.addAction(kSuiteCollab, "reset kCollabLay for conflict branch");
    st.deleteLevel(kCollabLay);
    if (!requireImportMany(git, R, kSuiteCollab, "layout_reset", T.ms(), kCollabLay, { basePath }, "importMany kCollabLay <- base")) {
        return;
    }
    if (!requireCommit(git, R, kSuiteCollab, "lay_x_commit", T.ms(), kCollabLay, "lay_x", levelAt(2), "commit lay_x")) {
        return;
    }
    R.addAction(kSuiteCollab, "build conflict payloads x111 x222");
    std::string conflictDec = levelAt(3);
    {
        auto stA = parseLevelString(levelAt(3));
        for (auto& [_, obj] : stA.objects) {
            obj.fields[key::kX] = "111";
        }
        conflictDec = serializeLevelString(stA);
    }
    std::string conflictDecB = levelAt(3);
    {
        auto stB = parseLevelString(levelAt(3));
        for (auto& [_, obj] : stB.objects) {
            obj.fields[key::kX] = "222";
        }
        conflictDecB = serializeLevelString(stB);
    }
    R.addAction(kSuiteCollab, "deleteLevel kDecA kDecB for conflict resets");
    st.deleteLevel(kDecA);
    st.deleteLevel(kDecB);
    if (!requireImportMany(git, R, kSuiteCollab, "dec_a_reset", T.ms(), kDecA, { basePath }, "importMany kDecA <- base conflict path")) {
        return;
    }
    if (!requireCommit(git, R, kSuiteCollab, "a_conflict_commit", T.ms(), kDecA, "a_conflict", conflictDec, "commit kDecA a_conflict")) {
        return;
    }
    if (!requireExport(git, R, kSuiteCollab, "export_a_conflict", T.ms(), kDecA, decAPath, fmt::format("export a_conflict {}", pathUtf8(decAPath)))) {
        return;
    }
    if (!requireImportMany(git, R, kSuiteCollab, "dec_b_reset", T.ms(), kDecB, { basePath }, "importMany kDecB <- base conflict path")) {
        return;
    }
    if (!requireCommit(git, R, kSuiteCollab, "b_conflict_commit", T.ms(), kDecB, "b_conflict", conflictDecB, "commit kDecB b_conflict")) {
        return;
    }
    if (!requireExport(git, R, kSuiteCollab, "export_b_conflict", T.ms(), kDecB, decBPath, fmt::format("export b_conflict {}", pathUtf8(decBPath)))) {
        return;
    }
    R.addAction(kSuiteCollab, "importMany merge overlapping conflicts");
    auto mergeConflict = git.importManyFromGdge(kCollabLay, { decAPath, decBPath });
    if (!mergeConflict.ok) {
        R.addFail(kSuiteCollab, "merge_conflict_import", mergeConflict.error, T.ms());
        return;
    }
    if (mergeConflict.value.conflictCount <= 0) {
        R.addFail(kSuiteCollab, "merge_conflict_count", fmt::format("conflictCount {}", mergeConflict.value.conflictCount), T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("mergeConflict conflictCount {}", mergeConflict.value.conflictCount));

    R.addPass(
        kSuiteCollab,
        "plan_and_merge",
        fmt::format("overlap conflicts {}", mergeConflict.value.conflictCount),
        T.ms()
    );
}

} // namespace git_editor
