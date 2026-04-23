#include "DbFlusher.hpp"
#include "DbZip.hpp"

#include "../store/CommitStore.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/utils/async.hpp>

#include <atomic>
#include <filesystem>
#include <mutex>

namespace git_editor {

namespace {

std::atomic<std::uint64_t> s_pendingVersion { 0 };
std::atomic<std::uint64_t> s_writtenVersion { 0 };
std::mutex                 s_zipWriteMutex;

// Perform the actual zip write with the mutex held.
// Drops the work if a newer version already landed.
void doZipWrite(ByteVector bytes, std::uint64_t version) {
    std::lock_guard lk(s_zipWriteMutex);
    if (version <= s_writtenVersion.load(std::memory_order_relaxed)) {
        return;
    }
    auto const dir = geode::Mod::get()->getSaveDir();
    auto const zip = dir / "git-editor.db.zip";
    if (!writeZipAtomic(zip, "git-editor.db", bytes)) {
        geode::log::error("DbFlusher: writeZipAtomic failed");
        return;
    }
    s_writtenVersion.store(version, std::memory_order_relaxed);
}

} // namespace

void scheduleLocalDbFlush() {
    if (!geode::Mod::get()->getSettingValue<bool>("compress-local-database")) {
        return;
    }

    auto* db = sharedCommitStore().rawHandle();
    if (!db) return;

    auto snapResult = sqliteBackupToMemory(db);
    if (!snapResult.ok) {
        geode::log::error("DbFlusher: snapshot failed: {}", snapResult.error);
        return;
    }

    auto const version = ++s_pendingVersion;
    auto bytes = std::move(snapResult.value);

    geode::async::runtime().spawnBlocking<void>(
        [bytes = std::move(bytes), version]() mutable {
            doZipWrite(std::move(bytes), version);
        }
    );
}

void flushLocalDbNow() {
    if (!geode::Mod::get()->getSettingValue<bool>("compress-local-database")) {
        return;
    }

    auto* db = sharedCommitStore().rawHandle();
    if (!db) return;

    auto snapResult = sqliteBackupToMemory(db);
    if (!snapResult.ok) {
        geode::log::error("DbFlusher::flushLocalDbNow: snapshot failed: {}", snapResult.error);
        return;
    }

    auto const version = ++s_pendingVersion;
    doZipWrite(std::move(snapResult.value), version);

    auto const dir = geode::Mod::get()->getSaveDir();
    auto const raw = dir / "git-editor.db";
    auto const wal = dir / "git-editor.db-wal";
    auto const shm = dir / "git-editor.db-shm";
    std::error_code ec;
    if (std::filesystem::exists(dir / "git-editor.db.zip", ec)) {
        std::filesystem::remove(raw, ec);
        std::filesystem::remove(wal, ec);
        std::filesystem::remove(shm, ec);
    }
}

} // namespace git_editor
