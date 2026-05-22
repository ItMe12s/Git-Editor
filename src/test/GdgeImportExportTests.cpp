#include "AutomatedTestHarness.hpp"

#include "util/format/StateHash.hpp"
#include "util/io/PathUtf8.hpp"

#include <fmt/format.h>

namespace git_editor {

void runGdgeExportImportTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    CompressExportGuard guard;
    R.addAction(kSuiteGdge, "CompressExportGuard ctor");
    if (!guard.ok()) {
        R.addAction(kSuiteGdge, "SKIP compress_toggle setting unavailable");
        ScopedTimer compressSkip;
        R.addSkip(kSuiteGdge, "compress_toggle", "ModSettingsManager or bool setting unavailable", compressSkip.ms());
        return;
    }
    R.addAction(kSuiteGdge, "compress guard armed");

    auto const rawPath = testDir / "at_raw.gdge";
    auto const zipPath = testDir / "at_zip.gdge";

    ScopedTimer fullSuite;

    R.addAction(kSuiteGdge, fmt::format("deleteLevel {} {} {}", kRawEx, kZipEx, kMix));
    ScopedTimer setupLevels;
    st.deleteLevel(kRawEx);
    st.deleteLevel(kZipEx);
    st.deleteLevel(kMix);
    R.addAction(kSuiteGdge, fmt::format("commit kRawEx kZipEx levelAt"));
    if (!requireCommit(git, R, kSuiteGdge, "setup_export_levels", setupLevels.ms(), kRawEx, "one", levelAt(1))
        || !requireCommit(git, R, kSuiteGdge, "setup_export_levels", setupLevels.ms(), kZipEx, "one", levelAt(2))) {
        return;
    }

    R.addAction(kSuiteGdge, "set compress false for raw export");
    ScopedTimer rawPhase;
    if (!guard.setValue(false)) {
        R.addFail(kSuiteGdge, "export_raw", "compress setting toggle failed", rawPhase.ms());
        return;
    }
    R.addAction(kSuiteGdge, fmt::format("exportLevelToGdge raw {}", pathUtf8(rawPath)));
    if (!requireExport(git, R, kSuiteGdge, "export_raw", rawPhase.ms(), kRawEx, rawPath)) {
        return;
    }
    auto rawSig = readFirstBytes(rawPath, 16);
    R.addAction(kSuiteGdge, fmt::format("raw first bytes len {} sqlite={}", rawSig.size(), startsWithSqlite(rawSig)));
    if (!startsWithSqlite(rawSig)) {
        R.addFail(kSuiteGdge, "raw_magic", "expected SQLite header", rawPhase.ms());
        return;
    }

    R.addAction(kSuiteGdge, "set compress true for zip export");
    ScopedTimer zipPhase;
    if (!guard.setValue(true)) {
        R.addFail(kSuiteGdge, "export_zip", "compress setting toggle failed", zipPhase.ms());
        return;
    }
    R.addAction(kSuiteGdge, fmt::format("exportLevelToGdge zip {}", pathUtf8(zipPath)));
    if (!requireExport(git, R, kSuiteGdge, "export_zip", zipPhase.ms(), kZipEx, zipPath)) {
        return;
    }
    auto zipSig = readFirstBytes(zipPath, 4);
    R.addAction(kSuiteGdge, fmt::format("zip pk={}", startsWithPk(zipSig)));
    if (!startsWithPk(zipSig)) {
        R.addFail(kSuiteGdge, "zip_magic", "expected PK header", zipPhase.ms());
        return;
    }

    R.addAction(kSuiteGdge, fmt::format("restore compress {}", guard.previousCompress()));
    static_cast<void>(guard.setValue(guard.previousCompress()));

    ScopedTimer importPhase;
    auto manyOpt = requireImportMany(
        git,
        R,
        kSuiteGdge,
        "import_mixed",
        importPhase.ms(),
        kMix,
        { rawPath, zipPath },
        "importManyFromGdge kMix rawPath+zipPath"
    );
    if (!manyOpt) {
        return;
    }
    auto const& many = *manyOpt;
    R.addAction(
        kSuiteGdge,
        fmt::format(
            "mergedCount {} smart {} sequential {} skipped {}",
            many.mergedCount,
            many.smartCount,
            many.sequentialCount,
            many.skippedCount
        )
    );
    if (many.mergedCount < 2) {
        R.addFail(
            kSuiteGdge,
            "import_mixed_counts",
            fmt::format("mergedCount {}", many.mergedCount),
            importPhase.ms()
        );
        return;
    }

    auto headMix = st.getHead(kMix);
    if (!headMix) {
        R.addFail(kSuiteGdge, "import_head", "no HEAD on kMix after import", importPhase.ms());
        return;
    }
    auto chainMix = chainOldestToNewest(st, kMix);
    if (chainMix.size() < 2) {
        R.addFail(
            kSuiteGdge,
            "import_chain_len",
            fmt::format("expected >= 2 commits got {}", chainMix.size()),
            importPhase.ms()
        );
        return;
    }
    auto reconMix = git.reconstruct(*headMix);
    if (!reconMix) {
        R.addFail(kSuiteGdge, "import_reconstruct", "reconstruct HEAD failed", importPhase.ms());
        return;
    }
    auto const payloadHash = hashLevelState(many.state);
    auto const dbHash      = hashLevelState(*reconMix);
    R.addAction(kSuiteGdge, fmt::format("import state hash payload={} db={}", payloadHash, dbHash));
    if (payloadHash != dbHash) {
        R.addFail(kSuiteGdge, "import_state_hash", "payload state != reconstructed HEAD", importPhase.ms());
        return;
    }
    auto const hashAgain = hashLevelState(*git.reconstruct(*headMix));
    if (hashAgain != dbHash) {
        R.addFail(kSuiteGdge, "import_state_stable", "hash changed on second reconstruct", importPhase.ms());
        return;
    }

    R.addPass(
        kSuiteGdge,
        "export_import_mixed",
        fmt::format(
            "smart={} sequential={} skipped={} commits={} hash={}",
            many.smartCount,
            many.sequentialCount,
            many.skippedCount,
            chainMix.size(),
            dbHash
        ),
        fullSuite.ms()
    );
}

} // namespace git_editor
