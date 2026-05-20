#include "AutomatedTestHarness.hpp"

#include "../util/format/StateHash.hpp"
#include "../util/io/PathUtf8.hpp"

#include <fmt/format.h>

namespace git_editor {

void runTwoPhaseTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    ScopedTimer suiteT;
    R.addAction(kSuiteTwoPhase, fmt::format("deleteLevel {}", kTwoPhase));
    st.deleteLevel(kTwoPhase);

    R.addAction(kSuiteTwoPhase, "commit tp1 tp2 tp3");
    auto a = git.commit(kTwoPhase, "tp1", levelAt(0));
    auto b = git.commit(kTwoPhase, "tp2", levelAt(10));
    auto c = git.commit(kTwoPhase, "tp3", levelAt(20));
    if (!a.ok || !b.ok || !c.ok) {
        R.addFail(kSuiteTwoPhase, "setup_three", "commit failed", suiteT.ms());
        return;
    }
    CommitId const c1 = a.value;
    CommitId const c3 = c.value;

    auto headBefore = st.getHead(kTwoPhase);
    auto const rowsBefore = st.list(kTwoPhase).size();
    if (!headBefore || *headBefore != c3) {
        R.addFail(kSuiteTwoPhase, "head_setup", "HEAD not on third commit", suiteT.ms());
        return;
    }

    R.addAction(kSuiteTwoPhase, fmt::format("prepareCheckout c1 {} head {}", c1, *headBefore));
    ScopedTimer checkoutPhase;
    auto prep = git.prepareCheckout(kTwoPhase, c1);
    if (!prep.result.ok) {
        R.addFail(kSuiteTwoPhase, "prepare_checkout", prep.result.error, checkoutPhase.ms());
        return;
    }
    if (!prep.pendingHead) {
        R.addFail(kSuiteTwoPhase, "prepare_pending", "expected pendingHead", checkoutPhase.ms());
        return;
    }

    auto headMid = st.getHead(kTwoPhase);
    auto const rowsMid = st.list(kTwoPhase).size();
    if (!headMid || *headMid != *headBefore || rowsMid != rowsBefore) {
        R.addFail(
            kSuiteTwoPhase,
            "prepare_no_db_write",
            fmt::format("head {}->{}, rows {}->{}", *headBefore, headMid ? *headMid : -1, rowsBefore, rowsMid),
            checkoutPhase.ms()
        );
        return;
    }
    R.addAction(kSuiteTwoPhase, "prepare left HEAD and row count unchanged");

    auto fin = git.finalizeCheckout(*prep.pendingHead, prep.result.value);
    if (!fin.ok) {
        R.addFail(kSuiteTwoPhase, "finalize_checkout", fin.error, checkoutPhase.ms());
        return;
    }
    auto headAfter = st.getHead(kTwoPhase);
    if (!headAfter || *headAfter == *headBefore) {
        R.addFail(kSuiteTwoPhase, "finalize_moves_head", "HEAD did not advance", checkoutPhase.ms());
        return;
    }
    if (st.list(kTwoPhase).size() != rowsBefore + 1) {
        R.addFail(kSuiteTwoPhase, "finalize_row_count", "expected one new commit row", checkoutPhase.ms());
        return;
    }
    auto applied = git.reconstruct(*headAfter);
    auto c1State = git.reconstruct(c1);
    if (!applied || !c1State || hashLevelState(*applied) != hashLevelState(*c1State)) {
        R.addFail(kSuiteTwoPhase, "finalize_state", "reconstructed state != target c1", checkoutPhase.ms());
        return;
    }
    R.addPass(kSuiteTwoPhase, "checkout_two_phase", "prepare/finalize split OK", checkoutPhase.ms());

    R.addAction(kSuiteTwoPhase, "checkout noop when HEAD equals target");
    ScopedTimer noopPhase;
    auto prepNoop = git.prepareCheckout(kTwoPhase, *headAfter);
    if (!prepNoop.result.ok) {
        R.addFail(kSuiteTwoPhase, "prepare_noop", prepNoop.result.error, noopPhase.ms());
        return;
    }
    if (prepNoop.pendingHead) {
        R.addFail(kSuiteTwoPhase, "prepare_noop_pending", "expected no pendingHead when HEAD==target", noopPhase.ms());
        return;
    }
    R.addPass(kSuiteTwoPhase, "checkout_noop", "no pending when already at target", noopPhase.ms());

    auto const gdgePath = testDir / "at_two_phase.gdge";
    R.addAction(kSuiteTwoPhase, fmt::format("export for import test {}", pathUtf8(gdgePath)));
    st.deleteLevel(kRawEx);
    if (!git.commit(kRawEx, "tp_export", levelAt(5)).ok) {
        R.addFail(kSuiteTwoPhase, "export_setup", "commit failed", suiteT.ms());
        return;
    }
    if (auto ex = git.exportLevelToGdge(kRawEx, gdgePath); !ex.ok) {
        R.addFail(kSuiteTwoPhase, "export_gdge", ex.error, suiteT.ms());
        return;
    }

    R.addAction(kSuiteTwoPhase, fmt::format("deleteLevel empty dest {}", kMix));
    st.deleteLevel(kMix);
    if (st.getHead(kMix).has_value()) {
        R.addFail(kSuiteTwoPhase, "mix_empty", "expected no HEAD on kMix", suiteT.ms());
        return;
    }

    R.addAction(kSuiteTwoPhase, "prepareImportManyFromGdge on empty dest");
    ScopedTimer importPhase;
    auto prepImp = git.prepareImportManyFromGdge(kMix, { gdgePath });
    if (!prepImp.result.ok) {
        R.addFail(kSuiteTwoPhase, "prepare_import", prepImp.result.error, importPhase.ms());
        return;
    }
    if (!prepImp.pendingMergeImport) {
        R.addFail(kSuiteTwoPhase, "prepare_import_pending", "expected pendingMergeImport", importPhase.ms());
        return;
    }
    if (st.getHead(kMix).has_value() || !st.list(kMix).empty()) {
        R.addFail(kSuiteTwoPhase, "prepare_import_no_write", "dest DB mutated before finalize", importPhase.ms());
        return;
    }

    auto finImp = git.finalizeImportManyFromGdge(*prepImp.pendingMergeImport, prepImp.result.value.state);
    if (!finImp.ok) {
        R.addFail(kSuiteTwoPhase, "finalize_import", finImp.error, importPhase.ms());
        return;
    }
    auto headMix = st.getHead(kMix);
    if (!headMix) {
        R.addFail(kSuiteTwoPhase, "finalize_import_head", "no HEAD after finalize", importPhase.ms());
        return;
    }
    auto mixRecon = git.reconstruct(*headMix);
    if (!mixRecon) {
        R.addFail(kSuiteTwoPhase, "finalize_import_recon", "reconstruct failed", importPhase.ms());
        return;
    }
    if (hashLevelState(*mixRecon) != hashLevelState(prepImp.result.value.state)) {
        R.addFail(kSuiteTwoPhase, "finalize_import_state", "DB state != prepared payload state", importPhase.ms());
        return;
    }
    R.addPass(kSuiteTwoPhase, "import_two_phase", "prepare/finalize import on empty dest OK", importPhase.ms());
}

} // namespace git_editor
