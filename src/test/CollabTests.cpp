#include "AutomatedTestHarness.hpp"

#include "../model/LevelParser.hpp"
#include "../util/PathUtf8.hpp"

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

    R.addAction(kSuiteCollab, "commit kCollabBase base");
    if (!git.commit(kCollabBase, "base", levelAt(0)).ok) {
        R.addFail(kSuiteCollab, "base_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("export base {}", pathUtf8(basePath)));
    if (auto ex = git.exportLevelToGdge(kCollabBase, basePath); !ex.ok) {
        R.addFail(kSuiteCollab, "export_base", ex.error, T.ms());
        return;
    }

    R.addAction(kSuiteCollab, "importMany kCollabLay <- base");
    if (auto m = git.importManyFromGdge(kCollabLay, { basePath }); !m.ok) {
        R.addFail(kSuiteCollab, "layout_import_base", m.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "commit kCollabLay lay_edit");
    if (!git.commit(kCollabLay, "lay_edit", levelAt(1)).ok) {
        R.addFail(kSuiteCollab, "layout_commit", "failed", T.ms());
        return;
    }

    R.addAction(kSuiteCollab, "importMany kDecA <- base");
    if (auto ma = git.importManyFromGdge(kDecA, { basePath }); !ma.ok) {
        R.addFail(kSuiteCollab, "dec_a_import", ma.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "commit kDecA a_edit");
    if (!git.commit(kDecA, "a_edit", levelAt(100)).ok) {
        R.addFail(kSuiteCollab, "dec_a_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("export decA {}", pathUtf8(decAPath)));
    if (auto ea = git.exportLevelToGdge(kDecA, decAPath); !ea.ok) {
        R.addFail(kSuiteCollab, "export_a", ea.error, T.ms());
        return;
    }

    R.addAction(kSuiteCollab, "importMany kDecB <- base");
    if (auto mb = git.importManyFromGdge(kDecB, { basePath }); !mb.ok) {
        R.addFail(kSuiteCollab, "dec_b_import", mb.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "commit kDecB b_edit");
    if (!git.commit(kDecB, "b_edit", levelAt(200)).ok) {
        R.addFail(kSuiteCollab, "dec_b_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("export decB {}", pathUtf8(decBPath)));
    if (auto eb = git.exportLevelToGdge(kDecB, decBPath); !eb.ok) {
        R.addFail(kSuiteCollab, "export_b", eb.error, T.ms());
        return;
    }

    R.addAction(kSuiteCollab, "reset kOther");
    st.deleteLevel(kOther);
    R.addAction(kSuiteCollab, "commit kOther other_root");
    if (!git.commit(kOther, "other_root", levelAt(9999)).ok) {
        R.addFail(kSuiteCollab, "other_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("export other {}", pathUtf8(otherPath)));
    if (auto eo = git.exportLevelToGdge(kOther, otherPath); !eo.ok) {
        R.addFail(kSuiteCollab, "export_other", eo.error, T.ms());
        return;
    }

    R.addAction(kSuiteCollab, "planImport decA decB");
    auto plan = git.planImport(kCollabLay, { decAPath, decBPath });
    if (plan.smart.size() != 2) {
        R.addFail(
            kSuiteCollab,
            "smart_pair",
            fmt::format("smart size {}", plan.smart.size()),
            T.ms()
        );
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
        R.addFail(
            kSuiteCollab,
            "merge_smart_count",
            fmt::format("smartCount {}", mergeTwo.value.smartCount),
            T.ms()
        );
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("merge_two smartCount {}", mergeTwo.value.smartCount));

    R.addAction(kSuiteCollab, "reset kCollabLay for conflict branch");
    st.deleteLevel(kCollabLay);
    R.addAction(kSuiteCollab, "importMany kCollabLay <- base");
    if (auto m0 = git.importManyFromGdge(kCollabLay, { basePath }); !m0.ok) {
        R.addFail(kSuiteCollab, "layout_reset", m0.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "commit lay_x");
    if (!git.commit(kCollabLay, "lay_x", levelAt(2)).ok) {
        R.addFail(kSuiteCollab, "lay_x_commit", "failed", T.ms());
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
    R.addAction(kSuiteCollab, "importMany kDecA <- base conflict path");
    if (auto ma2 = git.importManyFromGdge(kDecA, { basePath }); !ma2.ok) {
        R.addFail(kSuiteCollab, "dec_a_reset", ma2.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "commit kDecA a_conflict");
    if (!git.commit(kDecA, "a_conflict", conflictDec).ok) {
        R.addFail(kSuiteCollab, "a_conflict_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("export a_conflict {}", pathUtf8(decAPath)));
    if (auto ea2 = git.exportLevelToGdge(kDecA, decAPath); !ea2.ok) {
        R.addFail(kSuiteCollab, "export_a_conflict", ea2.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "importMany kDecB <- base conflict path");
    if (auto mb2 = git.importManyFromGdge(kDecB, { basePath }); !mb2.ok) {
        R.addFail(kSuiteCollab, "dec_b_reset", mb2.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "commit kDecB b_conflict");
    if (!git.commit(kDecB, "b_conflict", conflictDecB).ok) {
        R.addFail(kSuiteCollab, "b_conflict_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteCollab, fmt::format("export b_conflict {}", pathUtf8(decBPath)));
    if (auto eb2 = git.exportLevelToGdge(kDecB, decBPath); !eb2.ok) {
        R.addFail(kSuiteCollab, "export_b_conflict", eb2.error, T.ms());
        return;
    }
    R.addAction(kSuiteCollab, "importMany merge overlapping conflicts");
    auto mergeConflict = git.importManyFromGdge(kCollabLay, { decAPath, decBPath });
    if (!mergeConflict.ok) {
        R.addFail(kSuiteCollab, "merge_conflict_import", mergeConflict.error, T.ms());
        return;
    }
    if (mergeConflict.value.conflictCount <= 0) {
        R.addFail(
            kSuiteCollab,
            "merge_conflict_count",
            fmt::format("conflictCount {}", mergeConflict.value.conflictCount),
            T.ms()
        );
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
