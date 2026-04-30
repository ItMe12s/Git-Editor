#include "AutomatedTestHarness.hpp"

#include "../util/PathUtf8.hpp"

#include <fmt/format.h>

#include <Geode/loader/Mod.hpp>
#include <Geode/loader/ModSettingsManager.hpp>
#include <Geode/loader/SettingV3.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <future>
#include <functional>
#include <iomanip>
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

} // namespace

void ReportBuilder::addAction(std::string const& suite, std::string const& text) {
    out.actionLog.push_back(fmt::format("[{}] {}", suite, text));
}

void ReportBuilder::addPass(std::string const& suite, std::string const& name, std::string detail, double ms) {
    out.rows.push_back({ suite, name, "PASS", std::move(detail), ms });
    ++out.passCount;
}

void ReportBuilder::addFail(std::string const& suite, std::string const& name, std::string detail, double ms) {
    out.rows.push_back({ suite, name, "FAIL", std::move(detail), ms });
    ++out.failCount;
}

void ReportBuilder::addSkip(std::string const& suite, std::string const& name, std::string detail, double ms) {
    out.rows.push_back({ suite, name, "SKIP", std::move(detail), ms });
    ++out.skipCount;
}

ScopedTimer::ScopedTimer() : t0(std::chrono::steady_clock::now()) {}

double ScopedTimer::ms() const {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

CompressExportGuard::CompressExportGuard() {
    armed = setCompressExportFiles(true, &prev);
}

CompressExportGuard::~CompressExportGuard() {
    if (armed) {
        static_cast<void>(setCompressExportFiles(prev, nullptr));
    }
}

bool CompressExportGuard::setValue(bool compressed) {
    if (!armed) {
        return false;
    }
    return setCompressExportFiles(compressed, nullptr);
}

std::string levelAt(int x) {
    return fmt::format(";1,1,2,{},3,0", x);
}

std::string levelAtFixedX(int y) {
    return fmt::format(";1,1,2,10,3,{}", y);
}

namespace {

std::vector<LevelKey> allTestKeys() {
    return {
        kCheckout, kRevert, kSquash, kRawEx, kZipEx, kMix,
        kHistSrc, kHistDst, kCollabBase, kCollabLay, kDecA, kDecB, kOther
    };
}

} // namespace

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

    os << "actions:\n";
    if (s.actionLog.empty()) {
        os << "(none)\n";
    } else {
        for (auto const& line : s.actionLog) {
            os << line << '\n';
        }
    }
    os << '\n';

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

} // namespace git_editor
