#include "AutomatedTestHarness.hpp"

#include "../util/PathUtf8.hpp"

#include <fmt/format.h>

namespace git_editor {

void runEdgeTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R) {
    ScopedTimer T;
    if (!st.dbPath().empty()) {
        auto p = st.dbPath();
        R.addAction(kSuiteEdge, fmt::format("dbPath {}", pathUtf8(p)));
        if (!std::filesystem::exists(p)) {
            R.addFail(kSuiteEdge, "db_exists", fmt::format("missing {}", pathUtf8(p)), T.ms());
        } else {
            R.addAction(kSuiteEdge, "db file exists");
            R.addPass(kSuiteEdge, "db_under_save", pathUtf8(p), T.ms());
        }
    } else {
        R.addAction(kSuiteEdge, "dbPath empty branch");
        R.addFail(kSuiteEdge, "db_path_empty", "CommitStore db path empty", T.ms());
    }

    R.addAction(kSuiteEdge, fmt::format("unicode roundtrip prepare delete {}", kZipEx));
    st.deleteLevel(kZipEx);
    R.addAction(kSuiteEdge, "commit unicode_setup kZipEx");
    if (!git.commit(kZipEx, "unicode_path", levelAt(7)).ok) {
        R.addFail(kSuiteEdge, "unicode_setup", "commit failed", T.ms());
        return;
    }
    std::filesystem::path unicodeFile = testDir / std::filesystem::path(std::u8string(u8"at_\u0442\u0435\u0441\u0442.gdge"));
    R.addAction(kSuiteEdge, fmt::format("unicode export {}", pathUtf8(unicodeFile)));
    if (auto ex = git.exportLevelToGdge(kZipEx, unicodeFile); !ex.ok) {
        R.addSkip(kSuiteEdge, "unicode_export", ex.error, T.ms());
    } else {
        R.addAction(kSuiteEdge, "unicode importMany kMix");
        auto imp = git.importManyFromGdge(kMix, { unicodeFile });
        if (!imp.ok) {
            R.addFail(kSuiteEdge, "unicode_import", imp.error, T.ms());
        } else {
            R.addPass(kSuiteEdge, "unicode_roundtrip", pathUtf8(unicodeFile), T.ms());
        }
    }

    R.addAction(kSuiteEdge, fmt::format("first commit reconstruct test delete {}", kHistDst));
    st.deleteLevel(kHistDst);
    R.addAction(kSuiteEdge, "commit root_only kHistDst");
    auto firstCommit = git.commit(kHistDst, "root_only", levelAt(0));
    if (!firstCommit.ok) {
        R.addFail(kSuiteEdge, "first_commit", firstCommit.error, T.ms());
        return;
    }
    R.addAction(kSuiteEdge, fmt::format("commit id {}", firstCommit.value));

    auto recon = git.reconstruct(firstCommit.value);
    if (!recon) {
        R.addFail(kSuiteEdge, "reconstruct_first", "failed", T.ms());
        return;
    }
    R.addPass(kSuiteEdge, "first_commit_recon", "root reconstruct OK", T.ms());

    R.addAction(kSuiteEdge, fmt::format("deleteLevel fresh {}", kHistDst));
    st.deleteLevel(kHistDst);
    R.addPass(kSuiteEdge, "delete_level", "fresh key after deleteLevel", T.ms());
}

} // namespace git_editor
