#include "AutomatedTestHarness.hpp"

#include "../model/LevelParser.hpp"

#include <fmt/format.h>

namespace git_editor {

void runCollabPlanTest(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    ScopedTimer T;
    auto const basePath = testDir / "at_collab_base.gdge";
    auto const decAPath = testDir / "at_collab_a.gdge";
    auto const decBPath = testDir / "at_collab_b.gdge";
    auto const otherPath = testDir / "at_collab_other.gdge";

    for (auto k : { kCollabBase, kCollabLay, kDecA, kDecB, kOther }) {
        st.deleteLevel(k);
    }

    if (!git.commit(kCollabBase, "base", levelAt(0)).ok) {
        R.addFail(kSuiteCollab, "base_commit", "failed", T.ms());
        return;
    }
    if (auto ex = git.exportLevelToGdge(kCollabBase, basePath); !ex.ok) {
        R.addFail(kSuiteCollab, "export_base", ex.error, T.ms());
        return;
    }

    if (auto m = git.importManyFromGdge(kCollabLay, { basePath }); !m.ok) {
        R.addFail(kSuiteCollab, "layout_import_base", m.error, T.ms());
        return;
    }
    if (!git.commit(kCollabLay, "lay_edit", levelAt(1)).ok) {
        R.addFail(kSuiteCollab, "layout_commit", "failed", T.ms());
        return;
    }

    if (auto ma = git.importManyFromGdge(kDecA, { basePath }); !ma.ok) {
        R.addFail(kSuiteCollab, "dec_a_import", ma.error, T.ms());
        return;
    }
    if (!git.commit(kDecA, "a_edit", levelAt(100)).ok) {
        R.addFail(kSuiteCollab, "dec_a_commit", "failed", T.ms());
        return;
    }
    if (auto ea = git.exportLevelToGdge(kDecA, decAPath); !ea.ok) {
        R.addFail(kSuiteCollab, "export_a", ea.error, T.ms());
        return;
    }

    if (auto mb = git.importManyFromGdge(kDecB, { basePath }); !mb.ok) {
        R.addFail(kSuiteCollab, "dec_b_import", mb.error, T.ms());
        return;
    }
    if (!git.commit(kDecB, "b_edit", levelAt(200)).ok) {
        R.addFail(kSuiteCollab, "dec_b_commit", "failed", T.ms());
        return;
    }
    if (auto eb = git.exportLevelToGdge(kDecB, decBPath); !eb.ok) {
        R.addFail(kSuiteCollab, "export_b", eb.error, T.ms());
        return;
    }

    st.deleteLevel(kOther);
    if (!git.commit(kOther, "other_root", levelAt(9999)).ok) {
        R.addFail(kSuiteCollab, "other_commit", "failed", T.ms());
        return;
    }
    if (auto eo = git.exportLevelToGdge(kOther, otherPath); !eo.ok) {
        R.addFail(kSuiteCollab, "export_other", eo.error, T.ms());
        return;
    }

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

    st.deleteLevel(kCollabLay);
    if (auto m0 = git.importManyFromGdge(kCollabLay, { basePath }); !m0.ok) {
        R.addFail(kSuiteCollab, "layout_reset", m0.error, T.ms());
        return;
    }
    if (!git.commit(kCollabLay, "lay_x", levelAt(2)).ok) {
        R.addFail(kSuiteCollab, "lay_x_commit", "failed", T.ms());
        return;
    }
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
    st.deleteLevel(kDecA);
    st.deleteLevel(kDecB);
    if (auto ma2 = git.importManyFromGdge(kDecA, { basePath }); !ma2.ok) {
        R.addFail(kSuiteCollab, "dec_a_reset", ma2.error, T.ms());
        return;
    }
    if (!git.commit(kDecA, "a_conflict", conflictDec).ok) {
        R.addFail(kSuiteCollab, "a_conflict_commit", "failed", T.ms());
        return;
    }
    if (auto ea2 = git.exportLevelToGdge(kDecA, decAPath); !ea2.ok) {
        R.addFail(kSuiteCollab, "export_a_conflict", ea2.error, T.ms());
        return;
    }
    if (auto mb2 = git.importManyFromGdge(kDecB, { basePath }); !mb2.ok) {
        R.addFail(kSuiteCollab, "dec_b_reset", mb2.error, T.ms());
        return;
    }
    if (!git.commit(kDecB, "b_conflict", conflictDecB).ok) {
        R.addFail(kSuiteCollab, "b_conflict_commit", "failed", T.ms());
        return;
    }
    if (auto eb2 = git.exportLevelToGdge(kDecB, decBPath); !eb2.ok) {
        R.addFail(kSuiteCollab, "export_b_conflict", eb2.error, T.ms());
        return;
    }
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

    R.addPass(
        kSuiteCollab,
        "plan_and_merge",
        fmt::format("overlap conflicts {}", mergeConflict.value.conflictCount),
        T.ms()
    );
}

} // namespace git_editor
