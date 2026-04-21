#include "CommitStore.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>

#include <sqlite3.h>

#include <chrono>

namespace git_editor {

namespace {

constexpr char const* kSchemaSql = R"sql(
    CREATE TABLE IF NOT EXISTS commits (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        level_key  TEXT    NOT NULL,
        message    TEXT    NOT NULL,
        level_str  TEXT    NOT NULL,
        created_at INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_commits_level
        ON commits(level_key, created_at DESC);
)sql";

std::int64_t nowSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
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

    // sqlite3_open expects UTF-8. std::filesystem::path::string() is NOT
    // guaranteed UTF-8 on Windows (it can return the system narrow encoding),
    // so use u8string() and reinterpret - std::u8string::c_str returns
    // char8_t which is layout-compatible with char for SQLite's purposes.
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

    char* err = nullptr;
    rc = sqlite3_exec(m_db, kSchemaSql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        geode::log::error("git-editor: schema init failed: {}", err ? err : "?");
        if (err) sqlite3_free(err);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Reasonable defaults for a desktop/mobile mod: WAL keeps writes cheap,
    // NORMAL sync is durable enough for user-authored commits.
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;",    nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;",  nullptr, nullptr, nullptr);

    return true;
}

std::optional<std::int64_t> CommitStore::addCommit(
    std::string const& levelKey,
    std::string const& message,
    std::string const& levelString
) {
    if (!m_db) return std::nullopt;

    constexpr char const* sql =
        "INSERT INTO commits(level_key, message, level_str, created_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare addCommit failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }

    auto ts = nowSeconds();
    sqlite3_bind_text (st, 1, levelKey.c_str(),    static_cast<int>(levelKey.size()),    SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 2, message.c_str(),     static_cast<int>(message.size()),     SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 3, levelString.c_str(), static_cast<int>(levelString.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, ts);

    std::optional<std::int64_t> result;
    if (sqlite3_step(st) == SQLITE_DONE) {
        result = sqlite3_last_insert_rowid(m_db);
    } else {
        geode::log::error("git-editor: step addCommit failed: {}", sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(st);
    return result;
}

std::vector<Commit> CommitStore::listCommits(std::string const& levelKey) {
    std::vector<Commit> out;
    if (!m_db) return out;

    constexpr char const* sql =
        "SELECT id, message, created_at FROM commits "
        "WHERE level_key = ? ORDER BY created_at DESC, id DESC;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare listCommits failed: {}", sqlite3_errmsg(m_db));
        return out;
    }
    sqlite3_bind_text(st, 1, levelKey.c_str(), static_cast<int>(levelKey.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(st) == SQLITE_ROW) {
        Commit c;
        c.id        = sqlite3_column_int64(st, 0);
        c.levelKey  = levelKey;
        auto const* msg = reinterpret_cast<char const*>(sqlite3_column_text(st, 1));
        c.message   = msg ? msg : "";
        c.createdAt = sqlite3_column_int64(st, 2);
        out.push_back(std::move(c));
    }
    sqlite3_finalize(st);
    return out;
}

std::optional<Commit> CommitStore::getCommit(std::int64_t id) {
    if (!m_db) return std::nullopt;

    constexpr char const* sql =
        "SELECT id, level_key, message, level_str, created_at "
        "FROM commits WHERE id = ?;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("git-editor: prepare getCommit failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }
    sqlite3_bind_int64(st, 1, id);

    std::optional<Commit> result;
    if (sqlite3_step(st) == SQLITE_ROW) {
        Commit c;
        c.id        = sqlite3_column_int64(st, 0);
        auto const* key = reinterpret_cast<char const*>(sqlite3_column_text(st, 1));
        auto const* msg = reinterpret_cast<char const*>(sqlite3_column_text(st, 2));
        auto const* ls  = reinterpret_cast<char const*>(sqlite3_column_text(st, 3));
        c.levelKey     = key ? key : "";
        c.message      = msg ? msg : "";
        c.levelString  = ls  ? ls  : "";
        c.createdAt    = sqlite3_column_int64(st, 4);
        result = std::move(c);
    }
    sqlite3_finalize(st);
    return result;
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
            geode::log::error("git-editor: failed to open db at {}", dbPath.string());
        }
    }
    return store;
}

} // namespace git_editor
