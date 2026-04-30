#include "AutomatedTestHarness.hpp"

#include <fmt/format.h>

namespace git_editor {

void runRevertTests(GitService& git, CommitStore& st, ReportBuilder& R) {
    ScopedTimer T;
    R.addAction(kSuiteRevert, fmt::format("deleteLevel {}", kRevert));
    st.deleteLevel(kRevert);
    R.addAction(kSuiteRevert, "commit r1 r2 r3 r4");
    if (!git.commit(kRevert, "r1", levelAt(0)).ok
        || !git.commit(kRevert, "r2", levelAt(10)).ok
        || !git.commit(kRevert, "r3", levelAtFixedX(20)).ok
        || !git.commit(kRevert, "r4", levelAtFixedX(30)).ok) {
        R.addFail(kSuiteRevert, "setup_four", "commit failed", T.ms());
        return;
    }
    R.addAction(kSuiteRevert, "chainOldestToNewest");
    auto chain = chainOldestToNewest(st, kRevert);
    if (chain.size() != 4) {
        R.addFail(kSuiteRevert, "chain_len", fmt::format("expected 4 commits got {}", chain.size()), T.ms());
        return;
    }
    CommitId const c2 = chain[1];
    CommitId const c3 = chain[2];
    CommitId const c4 = chain[3];
    R.addAction(kSuiteRevert, fmt::format("commits c2={} c3={} c4={}", c2, c3, c4));

    R.addAction(kSuiteRevert, fmt::format("revert target c2 {}", c2));
    auto rev1 = git.revert(kRevert, c2);
    if (!rev1.ok) {
        R.addFail(kSuiteRevert, "revert_middle", rev1.error, T.ms());
        return;
    }
    if (!rev1.value.conflicts.empty()) {
        R.addFail(
            kSuiteRevert,
            "revert_no_conflict_expected",
            fmt::format("unexpected conflicts {}", rev1.value.conflicts.size()),
            T.ms()
        );
        return;
    }
    R.addAction(kSuiteRevert, fmt::format("revert revertCommit ok conflicts {}", rev1.value.conflicts.size()));

    auto rowsAfter = st.list(kRevert);
    bool hasC3 = false;
    bool hasC4 = false;
    for (auto const& r : rowsAfter) {
        if (r.id == c3) hasC3 = true;
        if (r.id == c4) hasC4 = true;
    }
    if (!hasC3 || !hasC4) {
        R.addFail(kSuiteRevert, "later_intact", "c3 or c4 missing after revert", T.ms());
        return;
    }
    R.addAction(kSuiteRevert, "c3 c4 still in list");

    auto headAfterRev = st.getHead(kRevert);
    if (!headAfterRev) {
        R.addFail(kSuiteRevert, "head_after_revert", "no HEAD", T.ms());
        return;
    }
    R.addAction(kSuiteRevert, fmt::format("HEAD after revert {}", *headAfterRev));

    R.addAction(kSuiteRevert, fmt::format("revert HEAD {}", *headAfterRev));
    auto rev2 = git.revert(kRevert, *headAfterRev);
    if (!rev2.ok) {
        R.addFail(kSuiteRevert, "revert_revert", rev2.error, T.ms());
        return;
    }
    R.addAction(kSuiteRevert, "double revert OK");
    R.addPass(kSuiteRevert, "revert_chain", "middle revert and double-revert OK", T.ms());
}

} // namespace git_editor
