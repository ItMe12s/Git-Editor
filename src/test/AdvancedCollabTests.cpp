#include "AutomatedTestHarness.hpp"

#include "../model/LevelParser.hpp"
#include "../util/PathUtf8.hpp"

#include <fmt/format.h>
#include <Geode/utils/file.hpp>

namespace git_editor {

namespace {

void deleteAdvancedLevels(CommitStore& st) {
    for (auto const k : {
             kAdvCollabBase,
             kAdvCollabIntegrator,
             kAdvCollabAlice,
             kAdvCollabBob,
             kAdvCollabScratch,
             kAdvCollabCara,
             kAdvCollabLegacy,
         }) {
        st.deleteLevel(k);
    }
}

bool ensureFixtureDir(std::filesystem::path const& p, ReportBuilder& R, ScopedTimer const& T) {
    auto mk = geode::utils::file::createDirectoryAll(p);
    if (mk.isErr()) {
        R.addFail(kSuiteAdvancedCollab, "mkdir_fixture", mk.unwrapErr(), T.ms());
        return false;
    }
    return true;
}

std::string allObjectsKeyX(LevelState&& stParsed, std::string const& xval) {
    for (auto& [_, obj] : stParsed.objects) {
        obj.fields[key::kX] = xval;
    }
    return serializeLevelString(stParsed);
}

} // namespace

void runAdvancedCollabSimulatorTests(
    GitService& git,
    CommitStore& st,
    std::filesystem::path const& testDir,
    ReportBuilder& R
) {
    ScopedTimer T;
    auto const fx = testDir / "advanced-collab";
    auto const basePath           = fx / "adv_collab_base.gdge";
    auto const alicePath          = fx / "adv_collab_alice.gdge";
    auto const bobPath            = fx / "adv_collab_bob.gdge";
    auto const scratchPath        = fx / "adv_collab_scratch.gdge";
    auto const legacyPath         = fx / "adv_collab_legacy.gdge";
    auto const bogusPath          = fx / "adv_collab_missing.gdge";
    auto const conflictBobExport  = fx / "adv_collab_conflict_b.gdge";
    auto const conflictCaraExport = fx / "adv_collab_conflict_c.gdge";

    R.addAction(kSuiteAdvancedCollab, "deleteLevel advanced keys");
    deleteAdvancedLevels(st);

    if (!ensureFixtureDir(fx, R, T)) {
        return;
    }

    if (!git.commit(kAdvCollabBase, "adv_base", levelAt(0)).ok) {
        R.addFail(kSuiteAdvancedCollab, "base_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteAdvancedCollab, fmt::format("export base {}", pathUtf8(basePath)));
    if (auto ex = git.exportLevelToGdge(kAdvCollabBase, basePath); !ex.ok) {
        R.addFail(kSuiteAdvancedCollab, "export_base", ex.error, T.ms());
        return;
    }

    R.addAction(kSuiteAdvancedCollab, "importMany integrator<-base");
    if (auto m0 = git.importManyFromGdge(kAdvCollabIntegrator, { basePath }); !m0.ok) {
        R.addFail(kSuiteAdvancedCollab, "integrator_import", m0.error, T.ms());
        return;
    }
    R.addAction(kSuiteAdvancedCollab, "commit integrator layout");
    if (!git.commit(kAdvCollabIntegrator, "integrator_layout", levelAt(1)).ok) {
        R.addFail(kSuiteAdvancedCollab, "integrator_commit", "failed", T.ms());
        return;
    }

    auto forkExport = [&](LevelKey const& k, std::filesystem::path const& outPath, int xField, char const* label) -> bool {
        R.addAction(kSuiteAdvancedCollab, fmt::format("importMany {}<-base {}", label, pathUtf8(basePath)));
        if (auto m = git.importManyFromGdge(k, { basePath }); !m.ok) {
            R.addFail(kSuiteAdvancedCollab, fmt::format("{}_import", label), m.error, T.ms());
            return false;
        }
        if (!git.commit(k, fmt::format("{}_edit", label), levelAt(xField)).ok) {
            R.addFail(kSuiteAdvancedCollab, fmt::format("{}_commit", label), "failed", T.ms());
            return false;
        }
        R.addAction(kSuiteAdvancedCollab, fmt::format("export {} {}", label, pathUtf8(outPath)));
        if (auto e = git.exportLevelToGdge(k, outPath); !e.ok) {
            R.addFail(kSuiteAdvancedCollab, fmt::format("{}_export", label), e.error, T.ms());
            return false;
        }
        return true;
    };

    if (!forkExport(kAdvCollabAlice, alicePath, 301, "alice")) {
        return;
    }
    if (!forkExport(kAdvCollabBob, bobPath, 302, "bob")) {
        return;
    }
    if (!forkExport(kAdvCollabScratch, scratchPath, 303, "scratch")) {
        return;
    }

    st.deleteLevel(kAdvCollabLegacy);
    R.addAction(kSuiteAdvancedCollab, "commit legacy divergent root");
    if (!git.commit(kAdvCollabLegacy, "legacy_root", levelAt(9000)).ok) {
        R.addFail(kSuiteAdvancedCollab, "legacy_commit", "failed", T.ms());
        return;
    }
    R.addAction(kSuiteAdvancedCollab, fmt::format("export legacy {}", pathUtf8(legacyPath)));
    if (auto el = git.exportLevelToGdge(kAdvCollabLegacy, legacyPath); !el.ok) {
        R.addFail(kSuiteAdvancedCollab, "export_legacy", el.error, T.ms());
        return;
    }

    R.addAction(kSuiteAdvancedCollab, "planImport alice+bob+scratch");
    auto planTriple = git.planImport(kAdvCollabIntegrator, { alicePath, bobPath, scratchPath });
    if (planTriple.smart.size() != 3 || !planTriple.sequential.empty()) {
        R.addFail(
            kSuiteAdvancedCollab,
            "plan_triple_smart",
            fmt::format("smart {} seq {}", planTriple.smart.size(), planTriple.sequential.size()),
            T.ms()
        );
        return;
    }
    R.addAction(kSuiteAdvancedCollab, "planImport alice+legacy");
    auto planMix = git.planImport(kAdvCollabIntegrator, { alicePath, legacyPath });
    if (planMix.smart.size() != 1 || planMix.sequential.size() != 1) {
        R.addFail(
            kSuiteAdvancedCollab,
            "plan_smart_vs_seq",
            fmt::format("smart {} seq {}", planMix.smart.size(), planMix.sequential.size()),
            T.ms()
        );
        return;
    }
    R.addPass(
        kSuiteAdvancedCollab,
        "advanced_plan_classification",
        fmt::format("triple smart {} mix smart {} seq {}", planTriple.smart.size(), planMix.smart.size(), planMix.sequential.size()),
        T.ms()
    );

    R.addAction(kSuiteAdvancedCollab, "importMany bogus+triple smart");
    auto batch = git.importManyFromGdge(kAdvCollabIntegrator, { bogusPath, alicePath, bobPath, scratchPath });
    if (!batch.ok) {
        R.addFail(kSuiteAdvancedCollab, "triple_import", batch.error, T.ms());
        return;
    }
    if (batch.value.skippedCount < 1) {
        R.addFail(
            kSuiteAdvancedCollab,
            "invalid_skip_count",
            fmt::format("skipped {}", batch.value.skippedCount),
            T.ms()
        );
        return;
    }
    if (batch.value.smartCount != 3) {
        R.addFail(
            kSuiteAdvancedCollab,
            "triple_merge_shape",
            fmt::format("smart {}", batch.value.smartCount),
            T.ms()
        );
        return;
    }
    R.addPass(
        kSuiteAdvancedCollab,
        "advanced_invalid_skip",
        fmt::format("skipped {}", batch.value.skippedCount),
        T.ms()
    );
    R.addPass(
        kSuiteAdvancedCollab,
        "advanced_batch_merge",
        fmt::format(
            "merged {} smart {} conflicts {}",
            batch.value.mergedCount,
            batch.value.smartCount,
            batch.value.conflictCount
        ),
        T.ms()
    );

    R.addAction(kSuiteAdvancedCollab, "importMany legacy sequential");
    auto seq = git.importManyFromGdge(kAdvCollabIntegrator, { legacyPath });
    if (!seq.ok) {
        R.addFail(kSuiteAdvancedCollab, "legacy_import", seq.error, T.ms());
        return;
    }
    if (seq.value.sequentialCount != 1 || seq.value.smartCount != 0) {
        R.addFail(
            kSuiteAdvancedCollab,
            "sequential_shape",
            fmt::format("smart {} seq {}", seq.value.smartCount, seq.value.sequentialCount),
            T.ms()
        );
        return;
    }
    R.addPass(
        kSuiteAdvancedCollab,
        "advanced_sequential_foreign",
        fmt::format("seq {} merged {}", seq.value.sequentialCount, seq.value.mergedCount),
        T.ms()
    );

    R.addAction(kSuiteAdvancedCollab, "reset integrator for conflict wave");
    st.deleteLevel(kAdvCollabIntegrator);
    if (auto m1 = git.importManyFromGdge(kAdvCollabIntegrator, { basePath }); !m1.ok) {
        R.addFail(kSuiteAdvancedCollab, "integrator_reset_import", m1.error, T.ms());
        return;
    }
    if (!git.commit(kAdvCollabIntegrator, "integrator_pre_conflict", levelAt(10)).ok) {
        R.addFail(kSuiteAdvancedCollab, "integrator_pre_cf_commit", "failed", T.ms());
        return;
    }

    std::string const conflictB = allObjectsKeyX(parseLevelString(levelAt(20)), "111");
    std::string const conflictC = allObjectsKeyX(parseLevelString(levelAt(20)), "222");

    st.deleteLevel(kAdvCollabBob);
    if (auto mb = git.importManyFromGdge(kAdvCollabBob, { basePath }); !mb.ok) {
        R.addFail(kSuiteAdvancedCollab, "cf_bob_import", mb.error, T.ms());
        return;
    }
    if (!git.commit(kAdvCollabBob, "bob_cf", conflictB).ok) {
        R.addFail(kSuiteAdvancedCollab, "bob_cf_commit", "failed", T.ms());
        return;
    }
    if (auto eb = git.exportLevelToGdge(kAdvCollabBob, conflictBobExport); !eb.ok) {
        R.addFail(kSuiteAdvancedCollab, "export_cf_bob", eb.error, T.ms());
        return;
    }

    st.deleteLevel(kAdvCollabCara);
    if (auto mc = git.importManyFromGdge(kAdvCollabCara, { basePath }); !mc.ok) {
        R.addFail(kSuiteAdvancedCollab, "cf_cara_import", mc.error, T.ms());
        return;
    }
    if (!git.commit(kAdvCollabCara, "cara_cf", conflictC).ok) {
        R.addFail(kSuiteAdvancedCollab, "cara_cf_commit", "failed", T.ms());
        return;
    }
    if (auto ec = git.exportLevelToGdge(kAdvCollabCara, conflictCaraExport); !ec.ok) {
        R.addFail(kSuiteAdvancedCollab, "export_cf_cara", ec.error, T.ms());
        return;
    }

    R.addAction(kSuiteAdvancedCollab, "importMany overlapping conflicts");
    auto mergeCf = git.importManyFromGdge(kAdvCollabIntegrator, { conflictBobExport, conflictCaraExport });
    if (!mergeCf.ok) {
        R.addFail(kSuiteAdvancedCollab, "merge_cf_import", mergeCf.error, T.ms());
        return;
    }
    if (mergeCf.value.conflictCount <= 0) {
        R.addFail(
            kSuiteAdvancedCollab,
            "merge_cf_count",
            fmt::format("conflictCount {}", mergeCf.value.conflictCount),
            T.ms()
        );
        return;
    }
    R.addPass(
        kSuiteAdvancedCollab,
        "advanced_conflict_merge",
        fmt::format("conflictCount {}", mergeCf.value.conflictCount),
        T.ms()
    );

    auto chain = chainOldestToNewest(st, kAdvCollabIntegrator);
    if (chain.size() < 2) {
        R.addFail(
            kSuiteAdvancedCollab,
            "final_chain",
            fmt::format("commits {}", chain.size()),
            T.ms()
        );
        return;
    }
    R.addPass(
        kSuiteAdvancedCollab,
        "advanced_final_history",
        fmt::format("commits {}", chain.size()),
        T.ms()
    );
}

} // namespace git_editor
