#include "CommitSchema.hpp"

#include <Geode/loader/Log.hpp>

#include <cstdlib>
#include <string>

namespace git_editor::commit_schema {

namespace {

int readSchemaVersion(sqlite3* db) {
    sqlite3_stmt* st = nullptr;
    constexpr char const* check =
        "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_meta';";
    if (sqlite3_prepare_v2(db, check, -1, &st, nullptr) != SQLITE_OK) return -2;
    bool exists = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    if (!exists) return -2;

    constexpr char const* sel = "SELECT v FROM schema_meta WHERE k='version';";
    if (sqlite3_prepare_v2(db, sel, -1, &st, nullptr) != SQLITE_OK) return -1;
    int ver = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        auto const* text = reinterpret_cast<char const*>(sqlite3_column_text(st, 0));
        if (text) ver = std::atoi(text);
    }
    sqlite3_finalize(st);
    return ver;
}

bool dropSchemaObjects(sqlite3* db) {
    if (!execOrLog(db, "DROP TABLE IF EXISTS refs;")) return false;
    if (!execOrLog(db, "DROP TABLE IF EXISTS commits;")) return false;
    if (!execOrLog(db, "DROP TABLE IF EXISTS level_aliases;")) return false;
    if (!execOrLog(db, "DROP TABLE IF EXISTS schema_meta;")) return false;
    execOrLog(db, "DROP INDEX IF EXISTS idx_commits_level;");
    execOrLog(db, "DROP INDEX IF EXISTS idx_level_aliases_canonical;");
    return true;
}

bool createSchemaObjects(sqlite3* db) {
    constexpr char const* createSchema = R"sql(
        CREATE TABLE IF NOT EXISTS commits (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            level_key  TEXT    NOT NULL,
            parent_id  INTEGER REFERENCES commits(id),
            reverts_id INTEGER REFERENCES commits(id),
            message    TEXT    NOT NULL,
            created_at INTEGER NOT NULL,
            delta_blob BLOB    NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_commits_level
            ON commits(level_key, created_at DESC);

        CREATE TABLE IF NOT EXISTS refs (
            level_key TEXT PRIMARY KEY,
            head_id   INTEGER NOT NULL REFERENCES commits(id)
        );

        CREATE TABLE IF NOT EXISTS level_aliases (
            observed_key TEXT PRIMARY KEY,
            canonical_key TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            last_seen_at INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_level_aliases_canonical
            ON level_aliases(canonical_key);

        CREATE TABLE IF NOT EXISTS schema_meta (
            k TEXT PRIMARY KEY,
            v TEXT NOT NULL
        );
    )sql";
    return execOrLog(db, createSchema);
}

bool upsertSchemaVersion(sqlite3* db, int version) {
    sqlite3_stmt* st = nullptr;
    constexpr char const* upsert =
        "INSERT INTO schema_meta(k, v) VALUES('version', ?) "
        "ON CONFLICT(k) DO UPDATE SET v = excluded.v;";
    if (sqlite3_prepare_v2(db, upsert, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare meta upsert: {}", sqlite3_errmsg(db));
        return false;
    }
    auto const vs = std::to_string(version);
    sqlite3_bind_text(st, 1, vs.c_str(), static_cast<int>(vs.size()), SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

} // namespace

bool execOrLog(sqlite3* db, char const* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        geode::log::error("sql error: {}", err ? err : "?");
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

bool ensureSchema(sqlite3* db, int schemaVersion) {
    int const current = readSchemaVersion(db);
    bool const needWipe = (current < schemaVersion);
    if (needWipe) {
        geode::log::info("wiping DB (found schema {}, need {})", current, schemaVersion);
        if (!dropSchemaObjects(db)) return false;
    }
    if (!createSchemaObjects(db)) return false;
    return upsertSchemaVersion(db, schemaVersion);
}

DeferredFkTransaction::DeferredFkTransaction(sqlite3* db) : m_db(db) {}

DeferredFkTransaction::~DeferredFkTransaction() {
    this->rollback();
}

bool DeferredFkTransaction::begin() {
    if (!execOrLog(m_db, "BEGIN IMMEDIATE;")) return false;
    if (!execOrLog(m_db, "PRAGMA defer_foreign_keys = ON;")) {
        execOrLog(m_db, "ROLLBACK;");
        return false;
    }
    m_open = true;
    return true;
}

bool DeferredFkTransaction::commit() {
    if (!m_open) return false;
    if (!execOrLog(m_db, "COMMIT;")) {
        this->rollback();
        return false;
    }
    execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
    m_open = false;
    return true;
}

void DeferredFkTransaction::rollback() {
    if (!m_open) return;
    execOrLog(m_db, "ROLLBACK;");
    execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
    m_open = false;
}

} // namespace git_editor::commit_schema
