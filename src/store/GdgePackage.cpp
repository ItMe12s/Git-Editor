#include "GdgePackage.hpp"

#include "../diff/Delta.hpp"
#include "../util/io/BlobCodec.hpp"
#include "../util/io/DbZip.hpp"
#include "../util/io/FileAtomic.hpp"
#include "../util/io/PathUtf8.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>

#include <sqlite3.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

namespace git_editor {

namespace {

using SqlitePtr = sqlite3*;
using SqliteStmtPtr = sqlite3_stmt*;

std::int64_t gdgePackageNowSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

bool execSql(SqlitePtr db, char const* sql) {
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool bindText(SqliteStmtPtr st, int index, std::string const& value) {
    return sqlite3_bind_text(
               st, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT
           ) == SQLITE_OK;
}

bool setMeta(SqlitePtr db, std::string const& key, std::string const& value) {
    constexpr char const* sql =
        "INSERT INTO meta(k, v) VALUES(?, ?) "
        "ON CONFLICT(k) DO UPDATE SET v = excluded.v;";

    SqliteStmtPtr st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    bool const ok = bindText(st, 1, key) && bindText(st, 2, value) && sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

std::optional<std::string> getMeta(SqlitePtr db, std::string const& key) {
    constexpr char const* sql = "SELECT v FROM meta WHERE k = ?;";
    SqliteStmtPtr st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return std::nullopt;
    if (!bindText(st, 1, key)) {
        sqlite3_finalize(st);
        return std::nullopt;
    }
    std::optional<std::string> out;
    if (sqlite3_step(st) == SQLITE_ROW) {
        auto const* text = reinterpret_cast<char const*>(sqlite3_column_text(st, 0));
        out = text ? text : "";
    }
    sqlite3_finalize(st);
    return out;
}

bool parseInt64(std::string const& text, std::int64_t& out) {
    auto const* begin = text.data();
    auto const* end = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc() && ptr == end;
}

std::string validateDataReason(GdgePackageData const& data) {
    if (data.metadata.rootHash.empty()) return "metadata.rootHash empty";
    if (data.metadata.sourceLevelKey.empty()) return "metadata.sourceLevelKey empty";
    if (data.metadata.formatVersion.empty()) return "metadata.formatVersion empty";

    std::vector<std::int64_t> indexes;
    indexes.reserve(data.commits.size());
    for (auto const& c : data.commits) {
        indexes.push_back(c.commitIndex);
    }
    std::sort(indexes.begin(), indexes.end());
    for (std::size_t i = 0; i < indexes.size(); ++i) {
        if (indexes[i] != static_cast<std::int64_t>(i)) {
            return "commit_index sequence not contiguous from 0";
        }
    }

    auto isValidIndex = [&](std::optional<std::int64_t> idx) -> bool {
        if (!idx) return true;
        return *idx >= 0 && *idx < static_cast<std::int64_t>(data.commits.size());
    };

    for (auto const& c : data.commits) {
        if (!isValidIndex(c.parentIndex)) return "parent_index out of range";
        if (!isValidIndex(c.revertsIndex)) return "reverts_index out of range";
    }
    if (data.metadata.headIndex && !isValidIndex(data.metadata.headIndex)) {
        return "head_index out of range";
    }
    return {};
}

bool validateData(GdgePackageData const& data) {
    return validateDataReason(data).empty();
}

} // namespace

namespace {

bool writeGdgePackageSqlite(std::filesystem::path const& outPath,
                            GdgePackageData const&       data) {
    if (!validateData(data)) {
        geode::log::error("writeGdgePackageSqlite: validateData failed at {}", pathUtf8(outPath));
        return false;
    }

    SqlitePtr db = nullptr;
    auto const utf8 = pathUtf8(outPath);
    if (sqlite3_open_v2(
            utf8.c_str(),
            &db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            nullptr
        ) != SQLITE_OK) {
        geode::log::error(
            "writeGdgePackageSqlite: sqlite3_open_v2 failed for {}: {}",
            pathUtf8(outPath),
            db ? sqlite3_errmsg(db) : "db handle null"
        );
        if (db) sqlite3_close(db);
        return false;
    }

    bool ok = execSql(db, "PRAGMA foreign_keys=OFF;")
           && execSql(db, "PRAGMA journal_mode=DELETE;")
           && execSql(db, "BEGIN IMMEDIATE;")
           && execSql(db, "DROP TABLE IF EXISTS commits;")
           && execSql(db, "DROP TABLE IF EXISTS meta;")
           && execSql(
                  db,
                  "CREATE TABLE meta (k TEXT PRIMARY KEY, v TEXT NOT NULL);"
              )
           && execSql(
                  db,
                  "CREATE TABLE commits ("
                  "commit_index INTEGER PRIMARY KEY, "
                  "parent_index INTEGER, "
                  "reverts_index INTEGER, "
                  "message TEXT NOT NULL, "
                  "created_at INTEGER NOT NULL, "
                  "delta_blob BLOB NOT NULL"
                  ");"
              );

    if (ok) {
        std::int64_t exportedAt = data.metadata.exportedAt > 0 ? data.metadata.exportedAt : gdgePackageNowSeconds();
        ok = setMeta(db, "format_version", data.metadata.formatVersion)
          && setMeta(db, "root_hash", data.metadata.rootHash)
          && setMeta(db, "source_level_key", data.metadata.sourceLevelKey)
          && setMeta(db, "exported_at", std::to_string(exportedAt));
        if (ok && data.metadata.headIndex) {
            ok = setMeta(db, "head_index", std::to_string(*data.metadata.headIndex));
        }
    }

    if (ok) {
        constexpr char const* sql =
            "INSERT INTO commits(commit_index, parent_index, reverts_index, message, created_at, delta_blob) "
            "VALUES (?, ?, ?, ?, ?, ?);";
        SqliteStmtPtr st = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            for (auto const& c : data.commits) {
                sqlite3_reset(st);
                sqlite3_clear_bindings(st);
                sqlite3_bind_int64(st, 1, c.commitIndex);
                if (c.parentIndex) sqlite3_bind_int64(st, 2, *c.parentIndex);
                else sqlite3_bind_null(st, 2);
                if (c.revertsIndex) sqlite3_bind_int64(st, 3, *c.revertsIndex);
                else sqlite3_bind_null(st, 3);
                auto const stored = compressBlob(c.deltaBlob);
                if (!stored) {
                    geode::log::error("gdge export: compressBlob refused payload (size={})", c.deltaBlob.size());
                    ok = false;
                    break;
                }
                ok = bindText(st, 4, c.message)
                  && sqlite3_bind_int64(st, 5, c.createdAt) == SQLITE_OK
                  && sqlite3_bind_blob(st, 6, stored->data(), static_cast<int>(stored->size()), SQLITE_TRANSIENT) == SQLITE_OK
                  && sqlite3_step(st) == SQLITE_DONE;
                if (!ok) break;
            }
            sqlite3_finalize(st);
        }
    }

    if (ok) {
        if (!execSql(db, "COMMIT;")) {
            ok = false;
        }
    }
    if (!ok) {
        // If BEGIN never ran, ROLLBACK errors and clobbers sqlite3_errmsg.
        std::string const err = sqlite3_errmsg(db);
        if (sqlite3_get_autocommit(db) == 0) {
            (void)execSql(db, "ROLLBACK;");
        }
        geode::log::error("writeGdgePackageSqlite: transaction or schema step failed: {}", err);
    }

    sqlite3_close(db);
    return ok;
}

} // namespace

Result<void> writeGdgePackage(std::filesystem::path const& outPath, GdgePackageData const& data) {
    Result<void> result;
    bool const compressExports =
        geode::Mod::get()->getSettingValue<bool>("compress-export-files");

    if (!compressExports) {
        auto tmpPath = outPath;
        tmpPath += ".tmp";
        if (!writeGdgePackageSqlite(tmpPath, data)) {
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            result.error = "failed to write sqlite package";
            return result;
        }
        if (!replaceFileAtomic(tmpPath, outPath)) {
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            result.error = "failed to replace package file atomically";
            return result;
        }
        result.ok = true;
        return result;
    }

    // Use a path distinct from writeZipAtomic tmp suffix.
    // Same path must not go to sqlite and the zip writer.
    auto sqlitePath = outPath;
    sqlitePath += ".sqlite-tmp";

    if (!writeGdgePackageSqlite(sqlitePath, data)) {
        result.error = "failed to write sqlite package for zip export";
        return result;
    }

    auto readRes = geode::utils::file::readBinary(sqlitePath);
    if (readRes.isErr()) {
        geode::log::error(
            "writeGdgePackage: readBinary failed after sqlite ({}): {}",
            pathUtf8(sqlitePath),
            readRes.unwrapErr()
        );
        std::error_code ec2;
        std::filesystem::remove(sqlitePath, ec2);
        result.error = "failed to read intermediate sqlite for zip export";
        return result;
    }

    bool const wroteZip = writeZipAtomic(outPath, "package.gdge", readRes.unwrap());
    {
        std::error_code ec;
        std::filesystem::remove(sqlitePath, ec);
        if (ec) {
            geode::log::warn(
                "writeGdgePackage: could not remove intermediate sqlite at {}: {}",
                pathUtf8(sqlitePath),
                ec.message()
            );
        }
    }
    if (!wroteZip) {
        geode::log::error("writeGdgePackage: writeZipAtomic failed for {}", pathUtf8(outPath));
        result.error = "failed to write zip package";
        return result;
    }
    result.ok = true;
    return result;
}

namespace {

Result<GdgePackageData> readGdgePackageFromSqlitePath(
    std::filesystem::path const& sqlitePath,
    std::filesystem::path const& cleanupPath
) {
    Result<GdgePackageData> result;
    auto const doCleanup = [&]() {
        if (!cleanupPath.empty()) {
            std::error_code ec;
            std::filesystem::remove(cleanupPath, ec);
        }
    };

    SqlitePtr db = nullptr;
    auto const utf8 = pathUtf8(sqlitePath);
    if (sqlite3_open_v2(
            utf8.c_str(),
            &db,
            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
            nullptr
        ) != SQLITE_OK) {
        result.error = std::string("sqlite3_open_v2 failed: ")
            + (db ? sqlite3_errmsg(db) : "db handle null");
        if (db) sqlite3_close(db);
        doCleanup();
        return result;
    }

    auto closeDb = [&]() {
        if (db) sqlite3_close(db);
        db = nullptr;
    };
    auto fail = [&](std::string reason) -> Result<GdgePackageData> {
        Result<GdgePackageData> r;
        r.error = std::move(reason);
        closeDb();
        doCleanup();
        return r;
    };

    GdgePackageData out;
    out.metadata.formatVersion = getMeta(db, "format_version").value_or("");
    out.metadata.rootHash = getMeta(db, "root_hash").value_or("");
    out.metadata.sourceLevelKey = getMeta(db, "source_level_key").value_or("");

    {
        auto const exportedAtText = getMeta(db, "exported_at").value_or("0");
        std::int64_t exportedAt = 0;
        if (!parseInt64(exportedAtText, exportedAt)) {
            return fail("exported_at parse failed: '" + exportedAtText + "'");
        }
        out.metadata.exportedAt = exportedAt;
    }

    if (auto head = getMeta(db, "head_index")) {
        std::int64_t parsedHead = 0;
        if (!parseInt64(*head, parsedHead)) {
            return fail("head_index parse failed: '" + *head + "'");
        }
        out.metadata.headIndex = parsedHead;
    }

    constexpr char const* sql =
        "SELECT commit_index, parent_index, reverts_index, message, created_at, delta_blob "
        "FROM commits ORDER BY commit_index ASC;";
    SqliteStmtPtr st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        return fail(std::string("prepare commits select failed: ") + sqlite3_errmsg(db));
    }

    int rc = SQLITE_ROW;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        GdgePackageCommit commit;
        commit.commitIndex = sqlite3_column_int64(st, 0);
        if (sqlite3_column_type(st, 1) != SQLITE_NULL) commit.parentIndex = sqlite3_column_int64(st, 1);
        if (sqlite3_column_type(st, 2) != SQLITE_NULL) commit.revertsIndex = sqlite3_column_int64(st, 2);
        auto const* message = reinterpret_cast<char const*>(sqlite3_column_text(st, 3));
        commit.message = message ? message : "";
        commit.createdAt = sqlite3_column_int64(st, 4);
        auto const* data = static_cast<char const*>(sqlite3_column_blob(st, 5));
        int const len = sqlite3_column_bytes(st, 5);
        if (data && len > 0) {
            std::string stored(data, data + len);
            auto json = decompressBlob(stored);
            if (!json) {
                return fail(
                    "decompress delta_blob failed for commit_index="
                    + std::to_string(commit.commitIndex)
                );
            }
            commit.deltaBlob = std::move(*json);
        }
        out.commits.push_back(std::move(commit));
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        return fail(std::string("commits step failed: ") + sqlite3_errmsg(db));
    }

    closeDb();
    doCleanup();
    if (auto reason = validateDataReason(out); !reason.empty()) {
        result.error = "validateData failed: " + reason;
        return result;
    }
    result.value = std::move(out);
    result.ok = true;
    return result;
}

} // namespace

Result<GdgePackageData> readGdgePackage(std::filesystem::path const& path) {
    Result<GdgePackageData> result;

    std::error_code existsEc;
    if (!std::filesystem::exists(path, existsEc)) {
        result.error = "file does not exist";
        return result;
    }

    auto const form = peekDbFileForm(path);

    if (form == DbFileForm::Sqlite) {
        return readGdgePackageFromSqlitePath(path, {});
    }

    if (form == DbFileForm::Zip) {
        auto tmpPath = geode::Mod::get()->getTempDir()
            / (pathUtf8(path.stem()) + ".gdge.tmp");

        auto extractRes = extractZipToFile(path, tmpPath, "package.gdge");
        if (!extractRes.ok) {
            result.error = "zip extract failed: " + extractRes.error;
            return result;
        }
        return readGdgePackageFromSqlitePath(tmpPath, tmpPath);
    }

    result.error = "unknown file form (not zip or sqlite)";
    return result;
}

} // namespace git_editor
