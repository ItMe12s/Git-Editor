#include "AutomatedTestHarness.hpp"

#include "../util/StateHash.hpp"

#include <fmt/format.h>

namespace git_editor {

void runCheckoutTests(GitService& git, CommitStore& st, ReportBuilder& R) {
    ScopedTimer T;
    R.addAction(kSuiteCheckout, fmt::format("deleteLevel {}", kCheckout));
    st.deleteLevel(kCheckout);
    R.addAction(kSuiteCheckout, "commit at_m1 x=0");
    auto a = git.commit(kCheckout, "at_m1", levelAt(0));
    R.addAction(kSuiteCheckout, "commit at_m2 x=10");
    auto b = git.commit(kCheckout, "at_m2", levelAt(10));
    R.addAction(kSuiteCheckout, "commit at_m3 x=20");
    auto c = git.commit(kCheckout, "at_m3", levelAt(20));
    if (!a.ok || !b.ok || !c.ok) {
        R.addFail(kSuiteCheckout, "setup_three_commits", "commit chain failed", T.ms());
        return;
    }
    CommitId const c1 = a.value;
    CommitId const c2 = b.value;
    CommitId const c3 = c.value;
    R.addAction(kSuiteCheckout, fmt::format("commits c1={} c2={} c3={}", c1, c2, c3));

    auto head0 = st.getHead(kCheckout);
    if (!head0 || *head0 != c3) {
        R.addFail(kSuiteCheckout, "head_after_three", "HEAD not third commit", T.ms());
        return;
    }
    R.addAction(kSuiteCheckout, fmt::format("HEAD c3 {}", c3));

    R.addAction(kSuiteCheckout, fmt::format("reconstruct {}", c3));
    auto orig = git.reconstruct(c3);
    if (!orig) {
        R.addFail(kSuiteCheckout, "reconstruct_c3", "reconstruct failed", T.ms());
        return;
    }
    auto const origHash = hashLevelState(*orig);
    R.addAction(kSuiteCheckout, fmt::format("orig hash {}", origHash));

    R.addAction(kSuiteCheckout, fmt::format("checkout to c1 {}", c1));
    auto co1 = git.checkout(kCheckout, c1);
    if (!co1.ok) {
        R.addFail(kSuiteCheckout, "checkout_to_c1", co1.error, T.ms());
        return;
    }
    auto head1 = st.getHead(kCheckout);
    if (!head1 || *head1 == c3) {
        R.addFail(kSuiteCheckout, "head_after_checkout1", "HEAD did not move", T.ms());
        return;
    }
    R.addAction(kSuiteCheckout, fmt::format("HEAD after checkout1 {}", *head1));
    auto st1 = git.reconstruct(*head1);
    auto c1State = git.reconstruct(c1);
    if (!st1 || !c1State || hashLevelState(*st1) != hashLevelState(*c1State)) {
        R.addFail(kSuiteCheckout, "state_after_checkout1", "state mismatch vs c1", T.ms());
        return;
    }
    R.addAction(kSuiteCheckout, "state matches c1");

    R.addAction(kSuiteCheckout, fmt::format("checkout back c3 {}", c3));
    auto co2 = git.checkout(kCheckout, c3);
    if (!co2.ok) {
        R.addFail(kSuiteCheckout, "checkout_back_c3", co2.error, T.ms());
        return;
    }
    auto head2 = st.getHead(kCheckout);
    if (!head2) {
        R.addFail(kSuiteCheckout, "head_after_checkout2", "no HEAD", T.ms());
        return;
    }
    R.addAction(kSuiteCheckout, fmt::format("HEAD after checkout2 {}", *head2));

    auto finalSt = git.reconstruct(*head2);
    if (!finalSt || hashLevelState(*finalSt) != origHash) {
        R.addFail(kSuiteCheckout, "double_checkout_state", "final state drift vs original HEAD", T.ms());
        return;
    }
    R.addAction(kSuiteCheckout, fmt::format("final hash {}", hashLevelState(*finalSt)));

    auto rows = st.list(kCheckout);
    bool sawC3 = false;
    for (auto const& r : rows) {
        if (r.id == c3) {
            sawC3 = true;
            break;
        }
    }
    if (!sawC3) {
        R.addFail(kSuiteCheckout, "chain_keeps_c3", "c3 missing from store list", T.ms());
        return;
    }
    R.addAction(kSuiteCheckout, "list retains c3");
    R.addPass(kSuiteCheckout, "double_checkout", "HEAD chain and reconstruction OK", T.ms());
}

} // namespace git_editor
