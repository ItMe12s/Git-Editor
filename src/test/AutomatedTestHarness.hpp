#pragma once

#include "AutomatedTestRunner.hpp"

#include "../service/GitService.hpp"
#include "../store/CommitStore.hpp"

#include "../core/ImportPlan.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace git_editor {

inline constexpr char kSuiteCheckout[] = "Checkout";
inline constexpr char kSuiteRevert[]   = "Revert";
inline constexpr char kSuiteSquash[]   = "Squash";
inline constexpr char kSuiteGdge[]       = "ImportExport";
inline constexpr char kSuiteHistory[]  = "LoadLevelHistory";
inline constexpr char kSuiteCollab[]    = "Collab";
inline constexpr char kSuiteAdvancedCollab[] = "AdvancedCollab";
inline constexpr char kSuiteEdge[]       = "Edge";
inline constexpr char kSuiteTwoPhase[]   = "TwoPhase";
inline constexpr char kSuiteManual[]    = "ManualChecklist";

inline LevelKey const kCheckout{"__git_editor_at_checkout"};
inline LevelKey const kRevert{"__git_editor_at_revert"};
inline LevelKey const kSquash{"__git_editor_at_squash"};
inline LevelKey const kRawEx{"__git_editor_at_export_raw"};
inline LevelKey const kZipEx{"__git_editor_at_export_zip"};
inline LevelKey const kMix{"__git_editor_at_import_mix"};
inline LevelKey const kHistSrc{"__git_editor_at_hist_src"};
inline LevelKey const kHistDst{"__git_editor_at_hist_dst"};
inline LevelKey const kCollabBase{"__git_editor_at_collab_base"};
inline LevelKey const kCollabLay{"__git_editor_at_collab_layout"};
inline LevelKey const kDecA{"__git_editor_at_dec_a"};
inline LevelKey const kDecB{"__git_editor_at_dec_b"};
inline LevelKey const kOther{"__git_editor_at_other_root"};
inline LevelKey const kAdvCollabBase{"__git_editor_at_adv_collab_base"};
inline LevelKey const kAdvCollabIntegrator{"__git_editor_at_adv_collab_integrator"};
inline LevelKey const kAdvCollabAlice{"__git_editor_at_adv_collab_alice"};
inline LevelKey const kAdvCollabBob{"__git_editor_at_adv_collab_bob"};
inline LevelKey const kAdvCollabScratch{"__git_editor_at_adv_collab_scratch"};
inline LevelKey const kAdvCollabCara{"__git_editor_at_adv_collab_cara"};
inline LevelKey const kAdvCollabLegacy{"__git_editor_at_adv_collab_legacy"};
inline LevelKey const kTwoPhase{"__git_editor_at_two_phase"};

struct ReportBuilder {
    AutomatedTestSummary& out;

    void addAction(std::string const& suite, std::string const& text);
    void addPass(std::string const& suite, std::string const& name, std::string detail, double ms);
    void addFail(std::string const& suite, std::string const& name, std::string detail, double ms);
    void addSkip(std::string const& suite, std::string const& name, std::string detail, double ms);
};

struct ScopedTimer {
    std::chrono::steady_clock::time_point t0;
    ScopedTimer();
    double ms() const;
};

struct CompressExportGuard {
    bool armed = false;
    bool prev  = true;
    CompressExportGuard();
    ~CompressExportGuard();
    bool ok() const { return armed; }
    bool previousCompress() const { return prev; }
    bool setValue(bool compressed);
};

std::string levelAt(int x);
std::string levelAtFixedX(int y);
void wipeTestLevels(CommitStore& st);
std::vector<CommitId> chainOldestToNewest(CommitStore& st, LevelKey const& k);
std::string readFirstBytes(std::filesystem::path const& p, std::size_t n);
bool startsWithSqlite(std::string const& prefix);
bool startsWithPk(std::string const& prefix);

std::optional<CommitId> requireCommit(
    GitService& git,
    ReportBuilder& R,
    std::string_view suite,
    std::string_view failName,
    double elapsedMs,
    LevelKey const& key,
    std::string_view message,
    std::string const& levelData,
    std::string_view actionText = {}
);

bool requireExport(
    GitService& git,
    ReportBuilder& R,
    std::string_view suite,
    std::string_view failName,
    double elapsedMs,
    LevelKey const& key,
    std::filesystem::path const& outPath,
    std::string_view actionText = {}
);

std::optional<ImportManyPayload> requireImportMany(
    GitService& git,
    ReportBuilder& R,
    std::string_view suite,
    std::string_view failName,
    double elapsedMs,
    LevelKey const& dest,
    std::vector<std::filesystem::path> const& paths,
    std::string_view actionText = {}
);

bool forkExport(
    GitService& git,
    ReportBuilder& R,
    std::string_view suite,
    double elapsedMs,
    LevelKey const& key,
    std::filesystem::path const& basePath,
    std::filesystem::path const& outPath,
    int xField,
    char const* label
);

void formatReport(AutomatedTestSummary& s, std::filesystem::path const& saveDir, std::string const& modId);

void runCheckoutTests(GitService& git, CommitStore& st, ReportBuilder& R);
void runRevertTests(GitService& git, CommitStore& st, ReportBuilder& R);
void runSquashTests(GitService& git, CommitStore& st, ReportBuilder& R);
void runGdgeExportImportTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R);
void runHistoryCopyTest(GitService& git, CommitStore& st, ReportBuilder& R);
void runCollabPlanTest(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R);
void runAdvancedCollabSimulatorTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R);
void runEdgeTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R);
void runTwoPhaseTests(GitService& git, CommitStore& st, std::filesystem::path const& testDir, ReportBuilder& R);
void appendManualSkips(ReportBuilder& R);

} // namespace git_editor
