#include "AutomatedTestHarness.hpp"

#include <fmt/format.h>

namespace git_editor {

void runGdgeExportImportTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    CompressExportGuard guard;
    if (!guard.ok()) {
        R.addSkip(kSuiteGdge, "compress_toggle", "ModSettingsManager or bool setting unavailable", 0);
        return;
    }

    auto const rawPath = testDir / "at_raw.gdge";
    auto const zipPath = testDir / "at_zip.gdge";

    st.deleteLevel(kRawEx);
    st.deleteLevel(kZipEx);
    st.deleteLevel(kMix);
    if (!git.commit(kRawEx, "one", levelAt(1)).ok || !git.commit(kZipEx, "one", levelAt(2)).ok) {
        R.addFail(kSuiteGdge, "setup_export_levels", "commit failed", 0);
        return;
    }

    if (!guard.setValue(false)) {
        R.addFail(kSuiteGdge, "export_raw", "compress setting toggle failed", 0);
        return;
    }
    if (auto ex = git.exportLevelToGdge(kRawEx, rawPath); !ex.ok) {
        R.addFail(kSuiteGdge, "export_raw", ex.error, 0);
        return;
    }
    auto rawSig = readFirstBytes(rawPath, 16);
    if (!startsWithSqlite(rawSig)) {
        R.addFail(kSuiteGdge, "raw_magic", "expected SQLite header", 0);
        return;
    }

    if (!guard.setValue(true)) {
        R.addFail(kSuiteGdge, "export_zip", "compress setting toggle failed", 0);
        return;
    }
    if (auto ex2 = git.exportLevelToGdge(kZipEx, zipPath); !ex2.ok) {
        R.addFail(kSuiteGdge, "export_zip", ex2.error, 0);
        return;
    }
    auto zipSig = readFirstBytes(zipPath, 4);
    if (!startsWithPk(zipSig)) {
        R.addFail(kSuiteGdge, "zip_magic", "expected PK header", 0);
        return;
    }

    static_cast<void>(guard.setValue(guard.previousCompress()));

    auto many = git.importManyFromGdge(kMix, { rawPath, zipPath });
    if (!many.ok) {
        R.addFail(kSuiteGdge, "import_mixed", many.error, 0);
        return;
    }
    if (many.value.mergedCount < 2) {
        R.addFail(
            kSuiteGdge,
            "import_mixed_counts",
            fmt::format("mergedCount {}", many.value.mergedCount),
            0
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
        0
    );
}

} // namespace git_editor
