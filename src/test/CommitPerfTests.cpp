#include "AutomatedTestHarness.hpp"

#include "model/LevelParser.hpp"

#include <fmt/format.h>

#include <cstdlib>

namespace git_editor {

namespace {

std::string buildHomogeneousLevel(int objectCount) {
    std::string out = "1,1,2,0,3,0";
    out.reserve(static_cast<std::size_t>(objectCount) * 24 + 16);
    for (int i = 0; i < objectCount; ++i) {
        out.append(fmt::format(";1,1,2,{},3,{}", i % 1000, i / 1000));
    }
    return out;
}

} // namespace

void runCommitPerfTests(GitService& git, CommitStore& st, ReportBuilder& R) {
    constexpr int kObjectCount = 150000;
    constexpr double kSecondCommitBudgetMs = 120000.0;

    ScopedTimer total;
    R.addAction("Perf", fmt::format("deleteLevel {} seed {} objects", kPerf, kObjectCount));
    st.deleteLevel(kPerf);

    auto seedLevel = buildHomogeneousLevel(kObjectCount);
    R.addAction("Perf", "initial commit (seed HEAD)");
    ScopedTimer seedTimer;
    auto seed = git.commit(kPerf, "perf seed", seedLevel);
    if (!seed.ok) {
        R.addFail("Perf", "seed_commit", seed.error, total.ms());
        return;
    }
    R.addAction("Perf", fmt::format("seed commit {} ms", seedTimer.ms()));

    auto parsed = parseLevelString(seedLevel);
    int touched = 0;
    for (auto& [_, obj] : parsed.objects) {
        if (touched >= 5) break;
        auto xIt = obj.fields.find(key::kX);
        if (xIt == obj.fields.end()) continue;
        char* end = nullptr;
        double x = std::strtod(xIt->second.c_str(), &end);
        if (end == xIt->second.c_str()) continue;
        obj.fields[key::kX] = fmt::format("{:.0f}", x + 1.0);
        ++touched;
    }
    auto modified = serializeLevelString(parsed);

    R.addAction("Perf", "second commit (5 x-field edits)");
    ScopedTimer hotTimer;
    auto hot = git.commit(kPerf, "perf hot", modified);
    double const hotMs = hotTimer.ms();
    if (!hot.ok) {
        R.addFail("Perf", "hot_commit", hot.error, total.ms());
        return;
    }

    auto head = st.getHead(kPerf);
    auto recon = head ? git.reconstruct(*head) : LevelStatePtr{};
    if (!recon || recon->objects.size() != static_cast<std::size_t>(kObjectCount)) {
        R.addFail(
            "Perf",
            "hot_reconstruct",
            fmt::format("objects {}", recon ? recon->objects.size() : 0),
            total.ms()
        );
        return;
    }

    if (hotMs > kSecondCommitBudgetMs) {
        R.addFail(
            "Perf",
            "hot_commit_budget",
            fmt::format("hot commit {:.0f}ms > {:.0f}ms budget", hotMs, kSecondCommitBudgetMs),
            total.ms()
        );
        return;
    }

    R.addPass(
        "Perf",
        "commit_150k_hot_path",
        fmt::format("seed={:.0f}ms hot={:.0f}ms objects={}", seedTimer.ms(), hotMs, kObjectCount),
        total.ms()
    );
}

} // namespace git_editor
