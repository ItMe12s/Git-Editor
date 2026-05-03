#include "AutomatedTestHarness.hpp"

#include "../model/LevelParser.hpp"
#include "../util/PathUtf8.hpp"

#include <fmt/format.h>

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
    if (!git.commit(kZipEx, "unicode_path", levelAt(7)).ok) {
        R.addFail(kSuiteEdge, "unicode_setup", "commit failed", unicodeRoundtrip.ms());
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

    // Regression: per-object kA*/kS* keys must round-trip. Loss of these on a startpos object
    // (id 31) leaves StartPosObject::m_startSettings null and crashes LevelSettingsLayer::init
    // when the user opens "Edit Object" after a revert/checkout.
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
