#include "AutomatedTestHarness.hpp"

#include "../util/StateHash.hpp"

#include <fmt/format.h>
#include <optional>

namespace git_editor {

void runSquashTests(GitService& git, CommitStore& st, ReportBuilder& R) {
    ScopedTimer T;
    st.deleteLevel(kSquash);
    if (!git.commit(kSquash, "s1", levelAt(0)).ok
        || !git.commit(kSquash, "s2", levelAt(10)).ok
        || !git.commit(kSquash, "s3", levelAt(20)).ok
        || !git.commit(kSquash, "s4", levelAt(30)).ok) {
        R.addFail(kSuiteSquash, "setup_four", "commit failed", T.ms());
        return;
    }
    auto chain = chainOldestToNewest(st, kSquash);
    if (chain.size() != 4) {
        R.addFail(kSuiteSquash, "chain_len", fmt::format("got {}", chain.size()), T.ms());
        return;
    }
    CommitId const c2 = chain[1];
    CommitId const c3 = chain[2];
    auto headBefore = st.getHead(kSquash);
    auto reconHeadBefore = headBefore ? git.reconstruct(*headBefore) : std::nullopt;
    if (!reconHeadBefore) {
        R.addFail(kSuiteSquash, "recon_before", "reconstruct HEAD failed", T.ms());
        return;
    }
    auto const hashBefore = hashLevelState(*reconHeadBefore);

    auto sq = git.squash(kSquash, { c2, c3 }, "TEST_SQUASH_RANGE");
    if (!sq.ok) {
        R.addFail(kSuiteSquash, "squash_range", sq.error, T.ms());
        return;
    }
    bool foundRangeMsg = false;
    for (auto const& r : st.list(kSquash)) {
        if (r.message == "TEST_SQUASH_RANGE") {
            foundRangeMsg = true;
            break;
        }
    }
    if (!foundRangeMsg) {
        R.addFail(kSuiteSquash, "find_squash_row", "squash commit not found", T.ms());
        return;
    }
    auto chainAfterRange = chainOldestToNewest(st, kSquash);
    if (chainAfterRange.size() != 3) {
        R.addFail(
            kSuiteSquash,
            "count_after_range_squash",
            fmt::format("expected 3 got {}", chainAfterRange.size()),
            T.ms()
        );
        return;
    }
    auto headAfterRange = st.getHead(kSquash);
    auto reconAfterRange = headAfterRange ? git.reconstruct(*headAfterRange) : std::nullopt;
    if (!reconAfterRange || hashLevelState(*reconAfterRange) != hashBefore) {
        R.addFail(kSuiteSquash, "state_after_range_squash", "HEAD state drift after range squash", T.ms());
        return;
    }

    CommitId squashRangeId = 0;
    for (auto const& r : st.list(kSquash)) {
        if (r.message == "TEST_SQUASH_RANGE") {
            squashRangeId = r.id;
            break;
        }
    }
    if (squashRangeId == 0) {
        R.addFail(kSuiteSquash, "squash_range_id", "squash commit id not found", T.ms());
        return;
    }
    auto revSq = git.revert(kSquash, squashRangeId);
    if (!revSq.ok) {
        R.addFail(kSuiteSquash, "revert_range_squash", revSq.error, T.ms());
        return;
    }

    st.deleteLevel(kSquash);
    if (!git.commit(kSquash, "t1", levelAt(0)).ok
        || !git.commit(kSquash, "t2", levelAt(11)).ok
        || !git.commit(kSquash, "t3", levelAt(22)).ok) {
        R.addFail(kSuiteSquash, "setup_three_full", "commit failed", T.ms());
        return;
    }
    auto chainB = chainOldestToNewest(st, kSquash);
    if (chainB.size() != 3) {
        R.addFail(kSuiteSquash, "chain_b_len", fmt::format("got {}", chainB.size()), T.ms());
        return;
    }
    auto headB = st.getHead(kSquash);
    auto reconB = headB ? git.reconstruct(*headB) : std::nullopt;
    if (!reconB) {
        R.addFail(kSuiteSquash, "recon_b", "reconstruct failed", T.ms());
        return;
    }
    auto const hashB = hashLevelState(*reconB);

    auto sqFull = git.squash(kSquash, chainB, "TEST_SQUASH_ALL");
    if (!sqFull.ok) {
        R.addFail(kSuiteSquash, "squash_all", sqFull.error, T.ms());
        return;
    }
    auto chain3 = chainOldestToNewest(st, kSquash);
    if (chain3.size() != 1) {
        R.addFail(kSuiteSquash, "count_after_full_squash", fmt::format("expected 1 got {}", chain3.size()), T.ms());
        return;
    }
    auto headOne = st.getHead(kSquash);
    auto reconOne = headOne ? git.reconstruct(*headOne) : std::nullopt;
    if (!reconOne || hashLevelState(*reconOne) != hashB) {
        R.addFail(kSuiteSquash, "full_squash_state", "squashed-all state != HEAD hash before squash-all", T.ms());
        return;
    }
    R.addPass(
        kSuiteSquash,
        "squash_chain",
        "range squash stable, revert range squash ok, full squash matches HEAD hash",
        T.ms()
    );
}

} // namespace git_editor
