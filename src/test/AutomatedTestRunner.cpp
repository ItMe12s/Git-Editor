#include "AutomatedTestRunner.hpp"

#include "AutomatedTestHarness.hpp"

#include "../service/GitService.hpp"
#include "../store/CommitStore.hpp"

#include <fmt/format.h>
#include <Geode/utils/file.hpp>

#include <fstream>

namespace git_editor {

namespace {

struct FinallyWipeTestLevels {
    CommitStore& st;
    ReportBuilder* reporter;
    explicit FinallyWipeTestLevels(CommitStore& store, ReportBuilder& Rb) : st(store), reporter(&Rb) {}
    ~FinallyWipeTestLevels() {
        if (reporter) {
            reporter->addAction("Runner", "final wipeTestLevels(scope exit)");
        }
        wipeTestLevels(st);
    }
};

} // namespace

bool writeTextFileUtf8(std::filesystem::path const& path, std::string const& utf8) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return static_cast<bool>(out);
}

AutomatedTestSummary runAutomatedTests(std::filesystem::path const& saveDir, std::string const& modId) {
    AutomatedTestSummary summary;
    ReportBuilder R { summary };

    auto& st = sharedCommitStore();
    auto& git = sharedGitService();

    R.addAction("Runner", "runAutomatedTests begin");

    if (!st.dbPath().empty() && !std::filesystem::exists(st.dbPath())) {
        R.addAction("Init", "commit_store_db missing early return");
        R.addFail("Init", "commit_store_db", "DB path does not exist", 0);
        formatReport(summary, saveDir, modId);
        return summary;
    }
    if (st.dbPath().empty()) {
        R.addAction("Init", "CommitStore dbPath empty skipped existence check");
    } else {
        R.addAction("Init", "CommitStore dbPath exists");
    }

    R.addAction("Runner", "wipeTestLevels(initial)");
    wipeTestLevels(st);

    auto testDir = saveDir / "auto-test";
    R.addAction("Runner", "createDirectoryAll auto-test");
    if (auto mk = geode::utils::file::createDirectoryAll(testDir); mk.isErr()) {
        R.addAction("Init", fmt::format("mkdir_auto_test failed {}", mk.unwrapErr()));
        R.addFail("Init", "mkdir_auto_test", mk.unwrapErr(), 0);
        formatReport(summary, saveDir, modId);
        return summary;
    }
    R.addAction("Init", "auto-test directory ok");

    {
        FinallyWipeTestLevels cleanup(st, R);
        R.addAction("Runner", "suite Checkout");
        runCheckoutTests(git, st, R);
        R.addAction("Runner", "suite Revert");
        runRevertTests(git, st, R);
        R.addAction("Runner", "suite Squash");
        runSquashTests(git, st, R);
        R.addAction("Runner", "suite ImportExport");
        runGdgeExportImportTests(git, st, testDir, R);
        R.addAction("Runner", "suite LoadLevelHistory");
        runHistoryCopyTest(git, st, R);
        R.addAction("Runner", "suite Collab");
        runCollabPlanTest(git, st, testDir, R);
        R.addAction("Runner", "suite Edge");
        runEdgeTests(git, st, testDir, R);
        R.addAction("Runner", "suite ManualChecklist skips");
        appendManualSkips(R);
    }

    formatReport(summary, saveDir, modId);
    return summary;
}

} // namespace git_editor
