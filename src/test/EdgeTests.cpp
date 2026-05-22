#include "AutomatedTestHarness.hpp"

#include "../model/LevelParser.hpp"
#include "../service/CommitSummaryBuilder.hpp"
#include "../util/io/PathUtf8.hpp"

#include <fmt/format.h>

#include <fstream>

namespace git_editor {

void runEdgeTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    if (!st.dbPath().empty()) {
        ScopedTimer dbCheck;
        auto p = st.dbPath();
        R.addAction(kSuiteEdge, fmt::format("dbPath {}", pathUtf8(p)));
        if (!std::filesystem::exists(p)) {
            R.addFail(kSuiteEdge, "db_exists", fmt::format("missing {}", pathUtf8(p)), dbCheck.ms());
        } else {
            R.addAction(kSuiteEdge, "db file exists");
            R.addPass(kSuiteEdge, "db_under_save", pathUtf8(p), dbCheck.ms());
        }
    } else {
        ScopedTimer dbEmpty;
        R.addAction(kSuiteEdge, "dbPath empty branch");
        R.addFail(kSuiteEdge, "db_path_empty", "CommitStore db path empty", dbEmpty.ms());
    }

    R.addAction(kSuiteEdge, fmt::format("unicode roundtrip prepare delete {}", kZipEx));
    ScopedTimer unicodeRoundtrip;
    st.deleteLevel(kZipEx);
    R.addAction(kSuiteEdge, "commit unicode_setup kZipEx");
    if (!requireCommit(
            git,
            R,
            kSuiteEdge,
            "unicode_setup",
            unicodeRoundtrip.ms(),
            kZipEx,
            "unicode_path",
            levelAt(7)
        )) {
        return;
    }
    std::filesystem::path unicodeFile = testDir / std::filesystem::path(std::u8string(u8"at_\u0442\u0435\u0441\u0442.gdge"));
    R.addAction(kSuiteEdge, fmt::format("unicode export {}", pathUtf8(unicodeFile)));
    if (auto ex = git.exportLevelToGdge(kZipEx, unicodeFile); !ex.ok) {
        R.addSkip(kSuiteEdge, "unicode_export", ex.error, unicodeRoundtrip.ms());
    } else {
        R.addAction(kSuiteEdge, "unicode importMany kMix");
        auto imp = git.importManyFromGdge(kMix, { unicodeFile });
        if (!imp.ok) {
            R.addFail(kSuiteEdge, "unicode_import", imp.error, unicodeRoundtrip.ms());
        } else {
            R.addPass(kSuiteEdge, "unicode_roundtrip", pathUtf8(unicodeFile), unicodeRoundtrip.ms());
        }
    }

    R.addAction(kSuiteEdge, fmt::format("first commit reconstruct test delete {}", kHistDst));
    ScopedTimer firstCommitOnly;
    st.deleteLevel(kHistDst);
    R.addAction(kSuiteEdge, "commit root_only kHistDst");
    auto firstCommit = git.commit(kHistDst, "root_only", levelAt(0));
    if (!firstCommit.ok) {
        R.addFail(kSuiteEdge, "first_commit", firstCommit.error, firstCommitOnly.ms());
        return;
    }
    R.addAction(kSuiteEdge, fmt::format("commit id {}", firstCommit.value));

    ScopedTimer reconstructRoot;
    auto reconstructed = git.reconstruct(firstCommit.value);
    if (!reconstructed) {
        R.addFail(kSuiteEdge, "reconstruct_first", "failed", reconstructRoot.ms());
        return;
    }
    R.addPass(kSuiteEdge, "first_commit_recon", "root reconstruct OK", reconstructRoot.ms());

    R.addAction(kSuiteEdge, fmt::format("deleteLevel fresh {}", kHistDst));
    ScopedTimer deleteFresh;
    st.deleteLevel(kHistDst);
    R.addPass(kSuiteEdge, "delete_level", "fresh key after deleteLevel", deleteFresh.ms());

    R.addAction(kSuiteEdge, "listLevels absent after deleteLevel");
    ScopedTimer listLevelsT;
    bool foundHistDst = false;
    for (auto const& lv : st.listLevels()) {
        if (lv.levelKey == kHistDst) {
            foundHistDst = true;
            break;
        }
    }
    if (foundHistDst) {
        R.addFail(kSuiteEdge, "list_levels_after_delete", "deleted key still in listLevels", listLevelsT.ms());
        return;
    }
    R.addPass(kSuiteEdge, "list_levels_after_delete", "kHistDst absent from listLevels", listLevelsT.ms());

    R.addAction(kSuiteEdge, "updateMessage roundtrip");
    ScopedTimer updateMsgT;
    st.deleteLevel(kHistSrc);
    auto msgCommit = git.commit(kHistSrc, "original_msg", levelAt(1));
    if (!msgCommit.ok) {
        R.addFail(kSuiteEdge, "update_message_setup", msgCommit.error, updateMsgT.ms());
        return;
    }
    if (!st.updateMessage(msgCommit.value, "renamed_msg")) {
        R.addFail(kSuiteEdge, "update_message", "updateMessage returned false", updateMsgT.ms());
        return;
    }
    auto sums = buildCommitSummaries(st.listSummaryRows(kHistSrc));
    bool foundRenamed = false;
    for (auto const& s : sums) {
        if (s.id == msgCommit.value && s.message == "renamed_msg") {
            foundRenamed = true;
            break;
        }
    }
    if (!foundRenamed) {
        R.addFail(kSuiteEdge, "update_message_verify", "listSummaries missing renamed message", updateMsgT.ms());
        return;
    }
    R.addPass(kSuiteEdge, "update_message", "message updated in listSummaries", updateMsgT.ms());

    R.addAction(kSuiteEdge, "planImport noLocalCommits on empty dest");
    ScopedTimer planT;
    st.deleteLevel(kMix);
    auto const planPath = testDir / "at_plan_empty.gdge";
    st.deleteLevel(kRawEx);
    if (!requireCommit(git, R, kSuiteEdge, "plan_setup_commit", planT.ms(), kRawEx, "plan_probe", levelAt(3))) {
        return;
    }
    if (!requireExport(git, R, kSuiteEdge, "plan_setup_export", planT.ms(), kRawEx, planPath)) {
        return;
    }
    if (st.getHead(kMix).has_value()) {
        R.addFail(kSuiteEdge, "plan_empty_head", "kMix should have no HEAD", planT.ms());
        return;
    }
    auto plan = git.planImport(kMix, { planPath });
    if (!plan.noLocalCommits) {
        R.addFail(kSuiteEdge, "plan_no_local", "expected noLocalCommits true", planT.ms());
        return;
    }
    if (plan.smart.empty() && plan.sequential.empty()) {
        R.addFail(kSuiteEdge, "plan_buckets", "expected file in smart or sequential bucket", planT.ms());
        return;
    }
    R.addPass(kSuiteEdge, "plan_no_local_commits", "noLocalCommits set for empty dest", planT.ms());

    // Regression: each invalid import file must get its own reason.
    // The planner used to lump all failures into one bucket.
    R.addAction(kSuiteEdge, "plan invalid carries reason");
    ScopedTimer invalidReasonT;
    auto const missingPath = testDir / "at_does_not_exist.gdge";
    {
        std::error_code ec;
        std::filesystem::remove(missingPath, ec);
    }
    auto const garbagePath = testDir / "at_garbage.gdge";
    {
        std::ofstream out(garbagePath, std::ios::binary | std::ios::trunc);
        out << "not a gdge file";
    }
    auto invalidPlan = git.planImport(kRawEx, { missingPath, garbagePath });
    if (invalidPlan.invalid.size() != 2) {
        R.addFail(
            kSuiteEdge,
            "invalid_reason_count",
            fmt::format("expected 2 invalid got {}", invalidPlan.invalid.size()),
            invalidReasonT.ms()
        );
    } else if (invalidPlan.invalid[0].reason.empty() || invalidPlan.invalid[1].reason.empty()) {
        R.addFail(
            kSuiteEdge,
            "invalid_reason_empty",
            "expected non-empty reason on both invalid entries",
            invalidReasonT.ms()
        );
    } else if (invalidPlan.invalid[0].reason == invalidPlan.invalid[1].reason) {
        R.addFail(
            kSuiteEdge,
            "invalid_reason_distinct",
            fmt::format("expected distinct reasons got '{}'", invalidPlan.invalid[0].reason),
            invalidReasonT.ms()
        );
    } else {
        R.addPass(
            kSuiteEdge,
            "invalid_reason_populated",
            fmt::format(
                "missing='{}' garbage='{}'",
                invalidPlan.invalid[0].reason,
                invalidPlan.invalid[1].reason
            ),
            invalidReasonT.ms()
        );
    }

    // Regression: mixed valid and garbage files must still classify the valid one.
    // The garbage file must get its own reason.
    R.addAction(kSuiteEdge, "planImport mixed valid + garbage");
    ScopedTimer mixedBagT;
    auto mixedPlan = git.planImport(kRawEx, { planPath, garbagePath });
    auto const validCount = mixedPlan.smart.size() + mixedPlan.sequential.size();
    if (validCount != 1) {
        R.addFail(
            kSuiteEdge,
            "mixed_bag_valid",
            fmt::format(
                "expected 1 valid got smart={} sequential={}",
                mixedPlan.smart.size(),
                mixedPlan.sequential.size()
            ),
            mixedBagT.ms()
        );
    } else if (mixedPlan.invalid.size() != 1) {
        R.addFail(
            kSuiteEdge,
            "mixed_bag_invalid",
            fmt::format("expected 1 invalid got {}", mixedPlan.invalid.size()),
            mixedBagT.ms()
        );
    } else if (mixedPlan.invalid[0].path != garbagePath) {
        R.addFail(
            kSuiteEdge,
            "mixed_bag_path",
            fmt::format("invalid bucket holds wrong path: {}", pathUtf8(mixedPlan.invalid[0].path)),
            mixedBagT.ms()
        );
    } else if (mixedPlan.invalid[0].reason.empty()) {
        R.addFail(
            kSuiteEdge,
            "mixed_bag_reason",
            "garbage entry reason empty",
            mixedBagT.ms()
        );
    } else {
        R.addPass(
            kSuiteEdge,
            "mixed_bag_classify",
            fmt::format(
                "smart={} sequential={} invalid_reason='{}'",
                mixedPlan.smart.size(),
                mixedPlan.sequential.size(),
                mixedPlan.invalid[0].reason
            ),
            mixedBagT.ms()
        );
    }

    // Regression: kA and kS keys must round-trip on startpos objects.
    // Loss crashes LevelSettingsLayer when opening Edit Object after revert or checkout.
    R.addAction(kSuiteEdge, "startpos kA roundtrip");
    ScopedTimer startposRound;
    std::string startposLevel =
        "kS38,1_-1_2_104_3_104_4_-1_5_1_6_1000_7_1_15_1_18_0_8_1|;"
        "1,31,2,225,3,15,kA13,0,kA15,0,kA16,0,kA10,0,kA11,0,kA22,1;";
    auto stParsed = parseLevelString(startposLevel);
    bool foundStartPos = false;
    bool kAPreserved   = true;
    for (auto const& [_, obj] : stParsed.objects) {
        auto typeIt = obj.fields.find(key::kType);
        if (typeIt == obj.fields.end() || typeIt->second != "31") continue;
        foundStartPos = true;
        for (auto const* k : { "kA13", "kA15", "kA16", "kA10", "kA11", "kA22" }) {
            if (!obj.fields.contains(k)) {
                kAPreserved = false;
                break;
            }
        }
        break;
    }
    if (!foundStartPos) {
        R.addFail(kSuiteEdge, "startpos_roundtrip", "startpos object missing after parse", startposRound.ms());
    } else if (!kAPreserved) {
        R.addFail(kSuiteEdge, "startpos_roundtrip", "kA* keys lost during parse", startposRound.ms());
    } else {
        std::string reSerialized = serializeLevelString(stParsed);
        auto stReparsed = parseLevelString(reSerialized);
        bool match = stParsed.objects.size() == stReparsed.objects.size();
        for (auto const& [u, obj] : stParsed.objects) {
            auto it = stReparsed.objects.find(u);
            if (it == stReparsed.objects.end() || it->second.fields != obj.fields) {
                match = false;
                break;
            }
        }
        if (!match) {
            R.addFail(kSuiteEdge, "startpos_roundtrip", "fields drift across parse->serialize->parse", startposRound.ms());
        } else {
            R.addPass(kSuiteEdge, "startpos_roundtrip", "kA* keys preserved", startposRound.ms());
        }
    }
}

} // namespace git_editor
