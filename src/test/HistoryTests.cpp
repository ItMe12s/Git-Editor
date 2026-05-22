#include "AutomatedTestHarness.hpp"

#include "service/CommitSummaryBuilder.hpp"

#include <fmt/format.h>

namespace git_editor {

void runHistoryCopyTest(GitService& git, CommitStore& st, ReportBuilder& R) {
    ScopedTimer T;
    R.addAction(kSuiteHistory, fmt::format("deleteLevel {} {}", kHistSrc, kHistDst));
    st.deleteLevel(kHistSrc);
    st.deleteLevel(kHistDst);
    R.addAction(kSuiteHistory, "commit kHistSrc h1 h2 h3");
    if (!requireCommit(git, R, kSuiteHistory, "setup_src", T.ms(), kHistSrc, "h1", levelAt(0))
        || !requireCommit(git, R, kSuiteHistory, "setup_src", T.ms(), kHistSrc, "h2", levelAt(5))
        || !requireCommit(git, R, kSuiteHistory, "setup_src", T.ms(), kHistSrc, "h3", levelAt(9))) {
        return;
    }
    R.addAction(kSuiteHistory, "commit kHistDst d0");
    if (!requireCommit(git, R, kSuiteHistory, "setup_dst", T.ms(), kHistDst, "d0", levelAt(99))) {
        return;
    }

    R.addAction(kSuiteHistory, "importLevelFrom dst <- src");
    auto imp = git.importLevelFrom(kHistDst, kHistSrc);
    if (!imp.ok) {
        R.addFail(kSuiteHistory, "import_level_from", imp.error, T.ms());
        return;
    }
    auto srcS = buildCommitSummaries(st.listSummaryRows(kHistSrc));
    auto dstS = buildCommitSummaries(st.listSummaryRows(kHistDst));
    R.addAction(kSuiteHistory, fmt::format("listSummaries src {} dst {}", srcS.size(), dstS.size()));
    if (srcS.size() != dstS.size()) {
        R.addFail(
            kSuiteHistory,
            "summary_count",
            fmt::format("src {} dst {}", srcS.size(), dstS.size()),
            T.ms()
        );
        return;
    }
    R.addAction(kSuiteHistory, "compare message order");
    for (std::size_t i = 0; i < srcS.size(); ++i) {
        if (srcS[i].message != dstS[i].message) {
            R.addFail(kSuiteHistory, "message_order", fmt::format("index {}", i), T.ms());
            return;
        }
    }
    R.addPass(kSuiteHistory, "replace_history", "messages and count match", T.ms());
}

} // namespace git_editor
