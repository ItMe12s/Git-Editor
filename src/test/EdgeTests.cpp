#include "AutomatedTestHarness.hpp"

#include "../util/PathUtf8.hpp"

#include <fmt/format.h>

namespace git_editor {

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

} // namespace git_editor
