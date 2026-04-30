#include "AutomatedTestHarness.hpp"

#include "../util/StateHash.hpp"

namespace git_editor {

void runCheckoutTests(GitService& git, CommitStore& st, ReportBuilder& R) {
    ScopedTimer T;
    st.deleteLevel(kCheckout);
    auto a = git.commit(kCheckout, "at_m1", levelAt(0));
    auto b = git.commit(kCheckout, "at_m2", levelAt(10));
    auto c = git.commit(kCheckout, "at_m3", levelAt(20));
    if (!a.ok || !b.ok || !c.ok) {
        R.addFail(kSuiteCheckout, "setup_three_commits", "commit chain failed", T.ms());
        return;
    }
    CommitId const c1 = a.value;
    CommitId const c2 = b.value;
    CommitId const c3 = c.value;
    auto head0 = st.getHead(kCheckout);
    if (!head0 || *head0 != c3) {
        R.addFail(kSuiteCheckout, "head_after_three", "HEAD not third commit", T.ms());
        return;
    }
    auto orig = git.reconstruct(c3);
    if (!orig) {
        R.addFail(kSuiteCheckout, "reconstruct_c3", "reconstruct failed", T.ms());
        return;
    }
    auto const origHash = hashLevelState(*orig);

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
    auto st1 = git.reconstruct(*head1);
    auto c1State = git.reconstruct(c1);
    if (!st1 || !c1State || hashLevelState(*st1) != hashLevelState(*c1State)) {
        R.addFail(kSuiteCheckout, "state_after_checkout1", "state mismatch vs c1", T.ms());
        return;
    }

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
    auto finalSt = git.reconstruct(*head2);
    if (!finalSt || hashLevelState(*finalSt) != origHash) {
        R.addFail(kSuiteCheckout, "double_checkout_state", "final state drift vs original HEAD", T.ms());
        return;
    }
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
    R.addPass(kSuiteCheckout, "double_checkout", "HEAD chain and reconstruction OK", T.ms());
}

} // namespace git_editor
