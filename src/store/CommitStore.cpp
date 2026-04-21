#include "CommitStore.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>

#include <sqlite3.h>

#include <chrono>

namespace git_editor {

namespace {

std::int64_t nowSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// Returns -1 if no version row, parsed int otherwise. -2 indicates the
// schema_meta table itself does not exist yet.
int readSchemaVersion(sqlite3* db) {
    sqlite3_stmt* st = nullptr;
    // Does the meta table exist?
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

bool execOrLog(sqlite3* db, char const* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        geode::log::error("git-editor: sql error: {}", err ? err : "?");
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

} // namespace

CommitStore::~CommitStore() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool CommitStore::init(std::filesystem::path const& dbPath) {
    if (m_db) return true;

    auto u8 = dbPath.u8string();
    auto const* utf8 = reinterpret_cast<char const*>(u8.c_str());

    int rc = sqlite3_open_v2(
        utf8, &m_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr
    );
    if (rc != SQLITE_OK) {
        geode::log::error("git-editor: sqlite3_open failed: {}",
            m_db ? sqlite3_errmsg(m_db) : "<null>");
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return false;
    }

    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;",   nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;",    nullptr, nullptr, nullptr);

    if (!this->ensureSchema()) {
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }
    return true;
}

bool CommitStore::ensureSchema() {
    int const current = readSchemaVersion(m_db);

    // Migration policy: we only support a hard wipe from any prior
    // (snapshot-based) schema. The first real release using logical deltas
    // is version 2; earlier data is intentionally discarded.
    bool const needWipe = (current < kSchemaVersion);

    if (needWipe) {
        geode::log::info(
            "git-editor: wiping DB (found schema {}, need {})",
            current, kSchemaVersion
        );
        if (!execOrLog(m_db, "DROP TABLE IF EXISTS commits;"))      return false;
        if (!execOrLog(m_db, "DROP TABLE IF EXISTS refs;"))         return false;
        if (!execOrLog(m_db, "DROP TABLE IF EXISTS schema_meta;"))  return false;
        // Drop legacy indices that were created by the old snapshot schema.
        execOrLog(m_db, "DROP INDEX IF EXISTS idx_commits_level;");
    }

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

        CREATE TABLE IF NOT EXISTS schema_meta (
            k TEXT PRIMARY KEY,
            v TEXT NOT NULL
        );
    )sql";
    if (!execOrLog(m_db, createSchema)) return false;

    // Stamp the current schema version.
    sqlite3_stmt* st = nullptr;
    constexpr char const* upsert =
        "INSERT INTO schema_meta(k, v) VALUES('version', ?) "
        "ON CONFLICT(k) DO UPDATE SET v = excluded.v;";
    if (sqlite3_prepare_v2(m_db, upsert, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare meta upsert: {}", sqlite3_errmsg(m_db));
        return false;
    }
    auto vs = std::to_string(kSchemaVersion);
    sqlite3_bind_text(st, 1, vs.c_str(), static_cast<int>(vs.size()), SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

std::optional<CommitId> CommitStore::insert(
    LevelKey const& levelKey,
    std::optional<CommitId> parent,
    std::optional<CommitId> reverts,
    std::string const& message,
    std::string const& deltaBlob
) {
    if (!m_db) return std::nullopt;

    constexpr char const* sql =
        "INSERT INTO commits(level_key, parent_id, reverts_id, message, created_at, delta_blob) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare insert failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }

    sqlite3_bind_text(st, 1, levelKey.c_str(), static_cast<int>(levelKey.size()), SQLITE_TRANSIENT);
    if (parent)  sqlite3_bind_int64(st, 2, *parent);   else sqlite3_bind_null(st, 2);
    if (reverts) sqlite3_bind_int64(st, 3, *reverts);  else sqlite3_bind_null(st, 3);
    sqlite3_bind_text(st, 4, message.c_str(), static_cast<int>(message.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, nowSeconds());
    sqlite3_bind_blob(st, 6, deltaBlob.data(), static_cast<int>(deltaBlob.size()), SQLITE_TRANSIENT);

    std::optional<CommitId> out;
    if (sqlite3_step(st) == SQLITE_DONE) {
        out = sqlite3_last_insert_rowid(m_db);
    } else {
        geode::log::error("git-editor: insert step failed: {}", sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(st);
    return out;
}

namespace {

CommitRow rowFromStatement(sqlite3_stmt* st, bool includeBlob) {
    CommitRow r;
    r.id         = sqlite3_column_int64(st, 0);
    auto const* key = reinterpret_cast<char const*>(sqlite3_column_text(st, 1));
    r.levelKey   = key ? key : "";
    if (sqlite3_column_type(st, 2) != SQLITE_NULL) {
        r.parent = sqlite3_column_int64(st, 2);
    }
    if (sqlite3_column_type(st, 3) != SQLITE_NULL) {
        r.reverts = sqlite3_column_int64(st, 3);
    }
    auto const* msg = reinterpret_cast<char const*>(sqlite3_column_text(st, 4));
    r.message   = msg ? msg : "";
    r.createdAt = sqlite3_column_int64(st, 5);
    if (includeBlob) {
        auto const* data = static_cast<char const*>(sqlite3_column_blob(st, 6));
        int len = sqlite3_column_bytes(st, 6);
        if (data && len > 0) r.deltaBlob.assign(data, data + len);
    }
    return r;
}

} // namespace

std::optional<CommitRow> CommitStore::get(CommitId id) {
    if (!m_db) return std::nullopt;

    constexpr char const* sql =
        "SELECT id, level_key, parent_id, reverts_id, message, created_at, delta_blob "
        "FROM commits WHERE id = ?;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare get failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }
    sqlite3_bind_int64(st, 1, id);

    std::optional<CommitRow> out;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out = rowFromStatement(st, /*includeBlob*/ true);
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<CommitRow> CommitStore::list(LevelKey const& levelKey) {
    std::vector<CommitRow> out;
    if (!m_db) return out;

    constexpr char const* sql =
        "SELECT id, level_key, parent_id, reverts_id, message, created_at, '' AS delta_blob "
        "FROM commits WHERE level_key = ? "
        "ORDER BY created_at DESC, id DESC;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare list failed: {}", sqlite3_errmsg(m_db));
        return out;
    }
    sqlite3_bind_text(st, 1, levelKey.c_str(), static_cast<int>(levelKey.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(st) == SQLITE_ROW) {
        out.push_back(rowFromStatement(st, /*includeBlob*/ false));
    }
    sqlite3_finalize(st);
    return out;
}

std::optional<CommitId> CommitStore::getHead(LevelKey const& levelKey) {
    if (!m_db) return std::nullopt;

    constexpr char const* sql = "SELECT head_id FROM refs WHERE level_key = ?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare getHead failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }
    sqlite3_bind_text(st, 1, levelKey.c_str(), static_cast<int>(levelKey.size()), SQLITE_TRANSIENT);

    std::optional<CommitId> out;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    return out;
}

bool CommitStore::setHead(LevelKey const& levelKey, CommitId head) {
    if (!m_db) return false;

    constexpr char const* sql =
        "INSERT INTO refs(level_key, head_id) VALUES(?, ?) "
        "ON CONFLICT(level_key) DO UPDATE SET head_id = excluded.head_id;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare setHead failed: {}", sqlite3_errmsg(m_db));
        return false;
    }
    sqlite3_bind_text(st, 1, levelKey.c_str(), static_cast<int>(levelKey.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, head);

    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

CommitStore& sharedCommitStore() {
    static CommitStore store;
    static bool triedInit = false;
    if (!triedInit) {
        triedInit = true;
        auto dir = geode::Mod::get()->getSaveDir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        auto dbPath = dir / "git-editor.db";
        if (!store.init(dbPath)) {
            geode::log::error("git-editor: failed to open db at {}",
                reinterpret_cast<char const*>(dbPath.u8string().c_str()));
        }
    }
    return store;
}

} // namespace git_editor
