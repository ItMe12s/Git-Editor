#include "AutomatedTestHarness.hpp"

#include "../util/PathUtf8.hpp"

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
    if (!git.commit(kRawEx, "one", levelAt(1)).ok || !git.commit(kZipEx, "one", levelAt(2)).ok) {
        R.addFail(kSuiteGdge, "setup_export_levels", "commit failed", setupLevels.ms());
        return;
    }

    R.addAction(kSuiteGdge, "set compress false for raw export");
    ScopedTimer rawPhase;
    if (!guard.setValue(false)) {
        R.addFail(kSuiteGdge, "export_raw", "compress setting toggle failed", rawPhase.ms());
        return;
    }
    R.addAction(kSuiteGdge, fmt::format("exportLevelToGdge raw {}", pathUtf8(rawPath)));
    if (auto ex = git.exportLevelToGdge(kRawEx, rawPath); !ex.ok) {
        R.addFail(kSuiteGdge, "export_raw", ex.error, rawPhase.ms());
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
    if (auto ex2 = git.exportLevelToGdge(kZipEx, zipPath); !ex2.ok) {
        R.addFail(kSuiteGdge, "export_zip", ex2.error, zipPhase.ms());
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

    R.addAction(kSuiteGdge, "importManyFromGdge kMix rawPath+zipPath");
    ScopedTimer importPhase;
    auto many = git.importManyFromGdge(kMix, { rawPath, zipPath });
    if (!many.ok) {
        R.addFail(kSuiteGdge, "import_mixed", many.error, importPhase.ms());
        return;
    }
    R.addAction(
        kSuiteGdge,
        fmt::format(
            "mergedCount {} smart {} sequential {} skipped {}",
            many.value.mergedCount,
            many.value.smartCount,
            many.value.sequentialCount,
            many.value.skippedCount
        )
    );
    if (many.value.mergedCount < 2) {
        R.addFail(
            kSuiteGdge,
            "import_mixed_counts",
            fmt::format("mergedCount {}", many.value.mergedCount),
            importPhase.ms()
        );
        return;
    }
    R.addPass(
        kSuiteGdge,
        "export_import_mixed",
        fmt::format(
            "smart={} sequential={} skipped={}",
            many.value.smartCount,
            many.value.sequentialCount,
            many.value.skippedCount
        ),
        fullSuite.ms()
    );
}

} // namespace git_editor
