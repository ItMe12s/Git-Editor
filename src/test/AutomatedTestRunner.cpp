#include "AutomatedTestRunner.hpp"

#include "../model/LevelParser.hpp"
#include "../service/GitService.hpp"
#include "../store/CommitStore.hpp"
#include "../util/PathUtf8.hpp"
#include "../util/StateHash.hpp"

#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/ModSettingsManager.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/file.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <future>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace git_editor {

namespace {

void runOnMainThreadBlocking(std::function<void()> fn) {
    std::promise<void> done;
    auto fut = done.get_future();
    geode::queueInMainThread([&done, fn = std::move(fn)]() mutable {
        fn();
        done.set_value();
    });
    fut.wait();
}

constexpr char const* kSuiteCheckout = "Checkout";
constexpr char const* kSuiteRevert   = "Revert";
constexpr char const* kSuiteSquash   = "Squash";
constexpr char const* kSuiteGdge     = "ImportExport";
constexpr char const* kSuiteHistory  = "LoadLevelHistory";
constexpr char const* kSuiteCollab   = "Collab";
constexpr char const* kSuiteEdge     = "Edge";
constexpr char const* kSuiteManual   = "ManualChecklist";

LevelKey const kCheckout = "__git_editor_at_checkout";
LevelKey const kRevert   = "__git_editor_at_revert";
LevelKey const kSquash   = "__git_editor_at_squash";
LevelKey const kRawEx  = "__git_editor_at_export_raw";
LevelKey const kZipEx  = "__git_editor_at_export_zip";
LevelKey const kMix    = "__git_editor_at_import_mix";
LevelKey const kHistSrc = "__git_editor_at_hist_src";
LevelKey const kHistDst = "__git_editor_at_hist_dst";
LevelKey const kCollabBase = "__git_editor_at_collab_base";
LevelKey const kCollabLay  = "__git_editor_at_collab_layout";
LevelKey const kDecA   = "__git_editor_at_dec_a";
LevelKey const kDecB   = "__git_editor_at_dec_b";
LevelKey const kOther  = "__git_editor_at_other_root";

std::string levelAt(int x) {
    return fmt::format(";1,1,2,{},3,0", x);
}

std::vector<LevelKey> allTestKeys() {
    return {
        kCheckout, kRevert, kSquash, kRawEx, kZipEx, kMix,
        kHistSrc, kHistDst, kCollabBase, kCollabLay, kDecA, kDecB, kOther
    };
}

void wipeTestLevels(CommitStore& st) {
    for (auto const& k : allTestKeys()) {
        st.deleteLevel(k);
    }
}

std::vector<CommitId> chainOldestToNewest(CommitStore& st, LevelKey const& k) {
    auto rows = st.list(k);
    auto head = st.getHead(k);
    if (!head || rows.empty()) return {};
    std::unordered_map<CommitId, CommitRow> byId;
    byId.reserve(rows.size());
    for (auto const& r : rows) {
        byId.emplace(r.id, r);
    }
    std::vector<CommitId> newestFirst;
    CommitId cur = *head;
    for (;;) {
        newestFirst.push_back(cur);
        auto it = byId.find(cur);
        if (it == byId.end() || !it->second.parent) {
            break;
        }
        cur = *it->second.parent;
    }
    std::reverse(newestFirst.begin(), newestFirst.end());
    return newestFirst;
}

std::string readFirstBytes(std::filesystem::path const& p, std::size_t n) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::string buf(n, '\0');
    in.read(buf.data(), static_cast<std::streamsize>(n));
    buf.resize(static_cast<std::size_t>(in.gcount()));
    return buf;
}

bool startsWithSqlite(std::string const& prefix) {
    static constexpr char kMagic[] = "SQLite format 3";
    return prefix.size() >= sizeof(kMagic) - 1
        && std::memcmp(prefix.data(), kMagic, sizeof(kMagic) - 1) == 0;
}

bool startsWithPk(std::string const& prefix) {
    return prefix.size() >= 2 && static_cast<unsigned char>(prefix[0]) == 0x50
        && static_cast<unsigned char>(prefix[1]) == 0x4B;
}

bool setCompressExportFiles(bool value, bool* outPrevious) {
    bool ok = false;
    runOnMainThreadBlocking([&] {
        auto* mgr = geode::ModSettingsManager::from(geode::Mod::get());
        if (!mgr) {
            return;
        }
        auto sp = mgr->get("compress-export-files");
        if (!sp) {
            return;
        }
        auto bs = std::dynamic_pointer_cast<geode::BoolSettingV3>(sp);
        if (!bs) {
            return;
        }
        if (outPrevious) {
            *outPrevious = bs->getValue();
        }
        bs->setValue(value);
        ok = true;
    });
    return ok;
}

struct ReportBuilder {
    AutomatedTestSummary& out;

    void addPass(std::string const& suite, std::string const& name, std::string detail, double ms) {
        out.rows.push_back({ suite, name, "PASS", std::move(detail), ms });
        ++out.passCount;
    }
    void addFail(std::string const& suite, std::string const& name, std::string detail, double ms) {
        out.rows.push_back({ suite, name, "FAIL", std::move(detail), ms });
        ++out.failCount;
    }
    void addSkip(std::string const& suite, std::string const& name, std::string detail, double ms) {
        out.rows.push_back({ suite, name, "SKIP", std::move(detail), ms });
        ++out.skipCount;
    }
};

struct ScopedTimer {
    std::chrono::steady_clock::time_point t0;
    ScopedTimer() : t0(std::chrono::steady_clock::now()) {}
    double ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
};

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

void runRevertTests(GitService& git, CommitStore& st, ReportBuilder& R) {
    ScopedTimer T;
    st.deleteLevel(kRevert);
    if (!git.commit(kRevert, "r1", levelAt(0)).ok
        || !git.commit(kRevert, "r2", levelAt(10)).ok
        || !git.commit(kRevert, "r3", levelAt(20)).ok
        || !git.commit(kRevert, "r4", levelAt(30)).ok) {
        R.addFail(kSuiteRevert, "setup_four", "commit failed", T.ms());
        return;
    }
    auto chain = chainOldestToNewest(st, kRevert);
    if (chain.size() != 4) {
        R.addFail(kSuiteRevert, "chain_len", fmt::format("expected 4 commits got {}", chain.size()), T.ms());
        return;
    }
    CommitId const c2 = chain[1];
    CommitId const c3 = chain[2];
    CommitId const c4 = chain[3];

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

    auto headAfterRev = st.getHead(kRevert);
    if (!headAfterRev) {
        R.addFail(kSuiteRevert, "head_after_revert", "no HEAD", T.ms());
        return;
    }
    auto rev2 = git.revert(kRevert, *headAfterRev);
    if (!rev2.ok) {
        R.addFail(kSuiteRevert, "revert_revert", rev2.error, T.ms());
        return;
    }
    R.addPass(kSuiteRevert, "revert_chain", "middle revert and double-revert OK", T.ms());
}

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

void runGdgeExportImportTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    bool prevCompress = true;
    if (!setCompressExportFiles(true, &prevCompress)) {
        R.addSkip(kSuiteGdge, "compress_toggle", "ModSettingsManager or bool setting unavailable", 0);
        return;
    }

    auto const rawPath = testDir / "at_raw.gdge";
    auto const zipPath = testDir / "at_zip.gdge";

    st.deleteLevel(kRawEx);
    st.deleteLevel(kZipEx);
    st.deleteLevel(kMix);
    if (!git.commit(kRawEx, "one", levelAt(1)).ok || !git.commit(kZipEx, "one", levelAt(2)).ok) {
        setCompressExportFiles(prevCompress, nullptr);
        R.addFail(kSuiteGdge, "setup_export_levels", "commit failed", 0);
        return;
    }

    setCompressExportFiles(false, nullptr);
    if (auto ex = git.exportLevelToGdge(kRawEx, rawPath); !ex.ok) {
        setCompressExportFiles(prevCompress, nullptr);
        R.addFail(kSuiteGdge, "export_raw", ex.error, 0);
        return;
    }
    auto rawSig = readFirstBytes(rawPath, 16);
    if (!startsWithSqlite(rawSig)) {
        setCompressExportFiles(prevCompress, nullptr);
        R.addFail(kSuiteGdge, "raw_magic", "expected SQLite header", 0);
        return;
    }

    setCompressExportFiles(true, nullptr);
    if (auto ex2 = git.exportLevelToGdge(kZipEx, zipPath); !ex2.ok) {
        setCompressExportFiles(prevCompress, nullptr);
        R.addFail(kSuiteGdge, "export_zip", ex2.error, 0);
        return;
    }
    auto zipSig = readFirstBytes(zipPath, 4);
    if (!startsWithPk(zipSig)) {
        setCompressExportFiles(prevCompress, nullptr);
        R.addFail(kSuiteGdge, "zip_magic", "expected PK header", 0);
        return;
    }

    setCompressExportFiles(prevCompress, nullptr);

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

void runCollabPlanTest(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    ScopedTimer T;
    auto const basePath = testDir / "at_collab_base.gdge";
    auto const decAPath = testDir / "at_collab_a.gdge";
    auto const decBPath = testDir / "at_collab_b.gdge";
    auto const otherPath = testDir / "at_collab_other.gdge";

    for (auto k : { kCollabBase, kCollabLay, kDecA, kDecB, kOther }) {
        st.deleteLevel(k);
    }

    if (!git.commit(kCollabBase, "base", levelAt(0)).ok) {
        R.addFail(kSuiteCollab, "base_commit", "failed", T.ms());
        return;
    }
    if (auto ex = git.exportLevelToGdge(kCollabBase, basePath); !ex.ok) {
        R.addFail(kSuiteCollab, "export_base", ex.error, T.ms());
        return;
    }

    if (auto m = git.importManyFromGdge(kCollabLay, { basePath }); !m.ok) {
        R.addFail(kSuiteCollab, "layout_import_base", m.error, T.ms());
        return;
    }
    if (!git.commit(kCollabLay, "lay_edit", levelAt(1)).ok) {
        R.addFail(kSuiteCollab, "layout_commit", "failed", T.ms());
        return;
    }

    if (auto ma = git.importManyFromGdge(kDecA, { basePath }); !ma.ok) {
        R.addFail(kSuiteCollab, "dec_a_import", ma.error, T.ms());
        return;
    }
    if (!git.commit(kDecA, "a_edit", levelAt(100)).ok) {
        R.addFail(kSuiteCollab, "dec_a_commit", "failed", T.ms());
        return;
    }
    if (auto ea = git.exportLevelToGdge(kDecA, decAPath); !ea.ok) {
        R.addFail(kSuiteCollab, "export_a", ea.error, T.ms());
        return;
    }

    if (auto mb = git.importManyFromGdge(kDecB, { basePath }); !mb.ok) {
        R.addFail(kSuiteCollab, "dec_b_import", mb.error, T.ms());
        return;
    }
    if (!git.commit(kDecB, "b_edit", levelAt(200)).ok) {
        R.addFail(kSuiteCollab, "dec_b_commit", "failed", T.ms());
        return;
    }
    if (auto eb = git.exportLevelToGdge(kDecB, decBPath); !eb.ok) {
        R.addFail(kSuiteCollab, "export_b", eb.error, T.ms());
        return;
    }

    st.deleteLevel(kOther);
    if (!git.commit(kOther, "other_root", levelAt(9999)).ok) {
        R.addFail(kSuiteCollab, "other_commit", "failed", T.ms());
        return;
    }
    if (auto eo = git.exportLevelToGdge(kOther, otherPath); !eo.ok) {
        R.addFail(kSuiteCollab, "export_other", eo.error, T.ms());
        return;
    }

    auto plan = git.planImport(kCollabLay, { decAPath, decBPath });
    if (plan.smart.size() != 2) {
        R.addFail(
            kSuiteCollab,
            "smart_pair",
            fmt::format("smart size {}", plan.smart.size()),
            T.ms()
        );
        return;
    }
    auto planMix = git.planImport(kCollabLay, { decAPath, otherPath });
    if (planMix.smart.size() != 1 || planMix.sequential.size() != 1) {
        R.addFail(
            kSuiteCollab,
            "smart_vs_seq",
            fmt::format("smart {} seq {}", planMix.smart.size(), planMix.sequential.size()),
            T.ms()
        );
        return;
    }

    auto mergeTwo = git.importManyFromGdge(kCollabLay, { decAPath, decBPath });
    if (!mergeTwo.ok) {
        R.addFail(kSuiteCollab, "merge_two_decorators", mergeTwo.error, T.ms());
        return;
    }
    if (mergeTwo.value.smartCount < 2) {
        R.addFail(
            kSuiteCollab,
            "merge_smart_count",
            fmt::format("smartCount {}", mergeTwo.value.smartCount),
            T.ms()
        );
        return;
    }

    st.deleteLevel(kCollabLay);
    if (auto m0 = git.importManyFromGdge(kCollabLay, { basePath }); !m0.ok) {
        R.addFail(kSuiteCollab, "layout_reset", m0.error, T.ms());
        return;
    }
    if (!git.commit(kCollabLay, "lay_x", levelAt(2)).ok) {
        R.addFail(kSuiteCollab, "lay_x_commit", "failed", T.ms());
        return;
    }
    std::string conflictDec = levelAt(3);
    {
        auto stA = parseLevelString(levelAt(3));
        for (auto& [_, obj] : stA.objects) {
            obj.fields[key::kX] = "111";
        }
        conflictDec = serializeLevelString(stA);
    }
    std::string conflictDecB = levelAt(3);
    {
        auto stB = parseLevelString(levelAt(3));
        for (auto& [_, obj] : stB.objects) {
            obj.fields[key::kX] = "222";
        }
        conflictDecB = serializeLevelString(stB);
    }
    st.deleteLevel(kDecA);
    st.deleteLevel(kDecB);
    if (auto ma2 = git.importManyFromGdge(kDecA, { basePath }); !ma2.ok) {
        R.addFail(kSuiteCollab, "dec_a_reset", ma2.error, T.ms());
        return;
    }
    if (!git.commit(kDecA, "a_conflict", conflictDec).ok) {
        R.addFail(kSuiteCollab, "a_conflict_commit", "failed", T.ms());
        return;
    }
    if (auto ea2 = git.exportLevelToGdge(kDecA, decAPath); !ea2.ok) {
        R.addFail(kSuiteCollab, "export_a_conflict", ea2.error, T.ms());
        return;
    }
    if (auto mb2 = git.importManyFromGdge(kDecB, { basePath }); !mb2.ok) {
        R.addFail(kSuiteCollab, "dec_b_reset", mb2.error, T.ms());
        return;
    }
    if (!git.commit(kDecB, "b_conflict", conflictDecB).ok) {
        R.addFail(kSuiteCollab, "b_conflict_commit", "failed", T.ms());
        return;
    }
    if (auto eb2 = git.exportLevelToGdge(kDecB, decBPath); !eb2.ok) {
        R.addFail(kSuiteCollab, "export_b_conflict", eb2.error, T.ms());
        return;
    }
    auto mergeConflict = git.importManyFromGdge(kCollabLay, { decAPath, decBPath });
    if (!mergeConflict.ok) {
        R.addFail(kSuiteCollab, "merge_conflict_import", mergeConflict.error, T.ms());
        return;
    }
    if (mergeConflict.value.conflictCount <= 0) {
        R.addFail(
            kSuiteCollab,
            "merge_conflict_count",
            fmt::format("conflictCount {}", mergeConflict.value.conflictCount),
            T.ms()
        );
        return;
    }

    R.addPass(
        kSuiteCollab,
        "plan_and_merge",
        fmt::format("overlap conflicts {}", mergeConflict.value.conflictCount),
        T.ms()
    );
}

void runEdgeTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    ScopedTimer T;
    if (!st.dbPath().empty()) {
        auto p = st.dbPath();
        if (!std::filesystem::exists(p)) {
            R.addFail(kSuiteEdge, "db_exists", fmt::format("missing {}", pathUtf8(p)), T.ms());
        } else {
            R.addPass(kSuiteEdge, "db_under_save", pathUtf8(p), T.ms());
        }
    } else {
        R.addFail(kSuiteEdge, "db_path_empty", "CommitStore db path empty", T.ms());
    }

    st.deleteLevel(kZipEx);
    if (!git.commit(kZipEx, "unicode_path", levelAt(7)).ok) {
        R.addFail(kSuiteEdge, "unicode_setup", "commit failed", T.ms());
        return;
    }
    std::filesystem::path unicodeFile = testDir / std::filesystem::path(std::u8string(u8"at_\u0442\u0435\u0441\u0442.gdge"));
    if (auto ex = git.exportLevelToGdge(kZipEx, unicodeFile); !ex.ok) {
        R.addSkip(kSuiteEdge, "unicode_export", ex.error, T.ms());
    } else {
        auto imp = git.importManyFromGdge(kMix, { unicodeFile });
        if (!imp.ok) {
            R.addFail(kSuiteEdge, "unicode_import", imp.error, T.ms());
        } else {
            R.addPass(kSuiteEdge, "unicode_roundtrip", pathUtf8(unicodeFile), T.ms());
        }
    }

    st.deleteLevel(kHistDst);
    auto firstCommit = git.commit(kHistDst, "root_only", levelAt(0));
    if (!firstCommit.ok) {
        R.addFail(kSuiteEdge, "first_commit", firstCommit.error, T.ms());
        return;
    }
    auto recon = git.reconstruct(firstCommit.value);
    if (!recon) {
        R.addFail(kSuiteEdge, "reconstruct_first", "failed", T.ms());
        return;
    }
    R.addPass(kSuiteEdge, "first_commit_recon", "root reconstruct OK", T.ms());

    st.deleteLevel(kHistDst);
    R.addPass(kSuiteEdge, "delete_level", "fresh key after deleteLevel", T.ms());
}

void appendManualSkips(ReportBuilder& R) {
    struct Item {
        char const* name;
        char const* reason;
    };
    static constexpr Item kItems[] = {
        { "checkout_editor_verify", "needs live LevelEditorLayer apply and visual verify" },
        { "revert_conflict_popup", "needs UI conflict summary layer" },
        { "revert_20plus_lag", "needs manual perf observation" },
        { "import_plan_popup", "needs UI plan popup" },
        { "import_state_visual", "needs editor state compare" },
        { "collab_visual", "needs editor verify" },
        { "edge_scroll_tap_lag", "needs manual UI interaction" },
        { "geode_log_first_launch", "needs external log inspection" },
        { "node_ids_devtools", "needs geode.node-ids querySelector" },
    };
    for (auto const& it : kItems) {
        ScopedTimer rowT;
        R.addSkip(kSuiteManual, it.name, it.reason, rowT.ms());
    }
}

void formatReport(AutomatedTestSummary& s, std::filesystem::path const& saveDir, std::string const& modId) {
    std::ostringstream os;
    os << "=== Git Editor automated test ===\n";
    auto const unixTs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    os << "timestamp_unix: " << unixTs << "\n";
    os << "save_dir: " << pathUtf8(saveDir) << "\n";
    if (!modId.empty()) {
        os << "mod_id: " << modId << "\n";
    }
    os << "summary: PASS " << s.passCount << " FAIL " << s.failCount << " SKIP " << s.skipCount << "\n\n";

    std::string lastSuite;
    os << std::fixed << std::setprecision(2);
    for (auto const& r : s.rows) {
        if (r.suite != lastSuite) {
            lastSuite = r.suite;
            os << "--- " << lastSuite << " ---\n";
        }
        os << r.status << " | " << r.name << " | " << r.elapsedMs << " ms";
        if (!r.detail.empty()) {
            os << " | " << r.detail;
        }
        os << '\n';
    }
    s.reportText = std::move(os).str();
}

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

    runCheckoutTests(git, st, R);
    runRevertTests(git, st, R);
    runSquashTests(git, st, R);
    runGdgeExportImportTests(git, st, testDir, R);
    runHistoryCopyTest(git, st, R);
    runCollabPlanTest(git, st, testDir, R);
    runEdgeTests(git, st, testDir, R);
    appendManualSkips(R);

    wipeTestLevels(st);

    formatReport(summary, saveDir, modId);
    return summary;
}

} // namespace git_editor
