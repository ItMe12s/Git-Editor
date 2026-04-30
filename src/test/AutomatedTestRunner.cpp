#include "AutomatedTestRunner.hpp"

#include "AutomatedTestHarness.hpp"

#include "../service/GitService.hpp"
#include "../store/CommitStore.hpp"

#include <Geode/utils/file.hpp>

#include <fstream>

namespace git_editor {

namespace {

struct FinallyWipeTestLevels {
    CommitStore& st;
    explicit FinallyWipeTestLevels(CommitStore& store) : st(store) {}
    ~FinallyWipeTestLevels() { wipeTestLevels(st); }
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

    if (!st.dbPath().empty() && !std::filesystem::exists(st.dbPath())) {
        R.addFail("Init", "commit_store_db", "DB path does not exist", 0);
        formatReport(summary, saveDir, modId);
        return summary;
    }

    wipeTestLevels(st);

    auto testDir = saveDir / "auto-test";
    if (auto mk = geode::utils::file::createDirectoryAll(testDir); mk.isErr()) {
        R.addFail("Init", "mkdir_auto_test", mk.unwrapErr(), 0);
        formatReport(summary, saveDir, modId);
        return summary;
    }

    {
        FinallyWipeTestLevels cleanup(st);
        runCheckoutTests(git, st, R);
        runRevertTests(git, st, R);
        runSquashTests(git, st, R);
        runGdgeExportImportTests(git, st, testDir, R);
        runHistoryCopyTest(git, st, R);
        runCollabPlanTest(git, st, testDir, R);
        runEdgeTests(git, st, testDir, R);
        appendManualSkips(R);
    }

    formatReport(summary, saveDir, modId);
    return summary;
}

} // namespace git_editor
