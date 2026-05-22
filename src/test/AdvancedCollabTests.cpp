#include "AutomatedTestHarness.hpp"

#include "../model/LevelParser.hpp"
#include "../util/io/PathUtf8.hpp"

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

    if (!requireCommit(git, R, kSuiteAdvancedCollab, "base_commit", T.ms(), kAdvCollabBase, "adv_base", levelAt(0))) {
        return;
    }
    if (!requireExport(
            git,
            R,
            kSuiteAdvancedCollab,
            "export_base",
            T.ms(),
            kAdvCollabBase,
            basePath,
            fmt::format("export base {}", pathUtf8(basePath))
        )) {
        return;
    }
    if (!requireImportMany(
            git,
            R,
            kSuiteAdvancedCollab,
            "integrator_import",
            T.ms(),
            kAdvCollabIntegrator,
            { basePath },
            "importMany integrator<-base"
        )) {
        return;
    }
    if (!requireCommit(
            git,
            R,
            kSuiteAdvancedCollab,
            "integrator_commit",
            T.ms(),
            kAdvCollabIntegrator,
            "integrator_layout",
            levelAt(1),
            "commit integrator layout"
        )) {
        return;
    }

    if (!forkExport(git, R, kSuiteAdvancedCollab, T.ms(), kAdvCollabAlice, basePath, alicePath, 301, "alice")) {
        return;
    }
    if (!forkExport(git, R, kSuiteAdvancedCollab, T.ms(), kAdvCollabBob, basePath, bobPath, 302, "bob")) {
        return;
    }
    if (!forkExport(git, R, kSuiteAdvancedCollab, T.ms(), kAdvCollabScratch, basePath, scratchPath, 303, "scratch")) {
        return;
    }

    st.deleteLevel(kAdvCollabLegacy);
    if (!requireCommit(
            git,
            R,
            kSuiteAdvancedCollab,
            "legacy_commit",
            T.ms(),
            kAdvCollabLegacy,
            "legacy_root",
            levelAt(9000),
            "commit legacy divergent root"
        )) {
        return;
    }
    if (!requireExport(
            git,
            R,
            kSuiteAdvancedCollab,
            "export_legacy",
            T.ms(),
            kAdvCollabLegacy,
            legacyPath,
            fmt::format("export legacy {}", pathUtf8(legacyPath))
        )) {
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
        R.addFail(kSuiteAdvancedCollab, "invalid_skip_count", fmt::format("skipped {}", batch.value.skippedCount), T.ms());
        return;
    }
    if (batch.value.smartCount != 3) {
        R.addFail(kSuiteAdvancedCollab, "triple_merge_shape", fmt::format("smart {}", batch.value.smartCount), T.ms());
        return;
    }
    R.addPass(kSuiteAdvancedCollab, "advanced_invalid_skip", fmt::format("skipped {}", batch.value.skippedCount), T.ms());
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
    if (!requireImportMany(git, R, kSuiteAdvancedCollab, "integrator_reset_import", T.ms(), kAdvCollabIntegrator, { basePath })) {
        return;
    }
    if (!requireCommit(git, R, kSuiteAdvancedCollab, "integrator_pre_cf_commit", T.ms(), kAdvCollabIntegrator, "integrator_pre_conflict", levelAt(10))) {
        return;
    }

    std::string const conflictB = allObjectsKeyX(parseLevelString(levelAt(20)), "111");
    std::string const conflictC = allObjectsKeyX(parseLevelString(levelAt(20)), "222");

    st.deleteLevel(kAdvCollabBob);
    if (!requireImportMany(git, R, kSuiteAdvancedCollab, "cf_bob_import", T.ms(), kAdvCollabBob, { basePath })) {
        return;
    }
    if (!requireCommit(git, R, kSuiteAdvancedCollab, "bob_cf_commit", T.ms(), kAdvCollabBob, "bob_cf", conflictB)) {
        return;
    }
    if (!requireExport(git, R, kSuiteAdvancedCollab, "export_cf_bob", T.ms(), kAdvCollabBob, conflictBobExport)) {
        return;
    }

    st.deleteLevel(kAdvCollabCara);
    if (!requireImportMany(git, R, kSuiteAdvancedCollab, "cf_cara_import", T.ms(), kAdvCollabCara, { basePath })) {
        return;
    }
    if (!requireCommit(git, R, kSuiteAdvancedCollab, "cara_cf_commit", T.ms(), kAdvCollabCara, "cara_cf", conflictC)) {
        return;
    }
    if (!requireExport(git, R, kSuiteAdvancedCollab, "export_cf_cara", T.ms(), kAdvCollabCara, conflictCaraExport)) {
        return;
    }

    R.addAction(kSuiteAdvancedCollab, "importMany overlapping conflicts");
    auto mergeCf = git.importManyFromGdge(kAdvCollabIntegrator, { conflictBobExport, conflictCaraExport });
    if (!mergeCf.ok) {
        R.addFail(kSuiteAdvancedCollab, "merge_cf_import", mergeCf.error, T.ms());
        return;
    }
    if (mergeCf.value.conflictCount <= 0) {
        R.addFail(kSuiteAdvancedCollab, "merge_cf_count", fmt::format("conflictCount {}", mergeCf.value.conflictCount), T.ms());
        return;
    }
    R.addPass(kSuiteAdvancedCollab, "advanced_conflict_merge", fmt::format("conflictCount {}", mergeCf.value.conflictCount), T.ms());

    auto chain = chainOldestToNewest(st, kAdvCollabIntegrator);
    if (chain.size() < 2) {
        R.addFail(kSuiteAdvancedCollab, "final_chain", fmt::format("commits {}", chain.size()), T.ms());
        return;
    }
    R.addPass(kSuiteAdvancedCollab, "advanced_final_history", fmt::format("commits {}", chain.size()), T.ms());
}

} // namespace git_editor
