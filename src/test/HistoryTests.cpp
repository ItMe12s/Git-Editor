#include "AutomatedTestHarness.hpp"

#include <fmt/format.h>

namespace git_editor {

void runHistoryCopyTest(GitService& git, CommitStore& st, ReportBuilder& R) {
    ScopedTimer T;
    st.deleteLevel(kHistSrc);
    st.deleteLevel(kHistDst);
    if (!git.commit(kHistSrc, "h1", levelAt(0)).ok
        || !git.commit(kHistSrc, "h2", levelAt(5)).ok
        || !git.commit(kHistSrc, "h3", levelAt(9)).ok) {
        R.addFail(kSuiteHistory, "setup_src", "commit failed", T.ms());
        return;
    }
    if (!git.commit(kHistDst, "d0", levelAt(99)).ok) {
        R.addFail(kSuiteHistory, "setup_dst", "commit failed", T.ms());
        return;
    }
    auto imp = git.importLevelFrom(kHistDst, kHistSrc);
    if (!imp.ok) {
        R.addFail(kSuiteHistory, "import_level_from", imp.error, T.ms());
        return;
    }
    auto srcS = st.listSummaries(kHistSrc);
    auto dstS = st.listSummaries(kHistDst);
    if (srcS.size() != dstS.size()) {
        R.addFail(
            kSuiteHistory,
            "summary_count",
            fmt::format("src {} dst {}", srcS.size(), dstS.size()),
            T.ms()
        );
        return;
    }
    for (std::size_t i = 0; i < srcS.size(); ++i) {
        if (srcS[i].message != dstS[i].message) {
            R.addFail(kSuiteHistory, "message_order", fmt::format("index {}", i), T.ms());
            return;
        }
    }
    R.addPass(kSuiteHistory, "replace_history", "messages and count match", T.ms());
}

} // namespace git_editor
