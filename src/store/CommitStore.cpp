#include "CommitStore.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>

#include <sqlite3.h>

#include <chrono>
#include <charconv>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace git_editor {

namespace {

std::int64_t nowSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// -2 means no schema_meta, -1 no version row, else stored version int
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
        geode::log::error("sqlite3_open failed: {}",
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

    bool const needWipe = (current < kSchemaVersion);

    if (needWipe) {
        geode::log::info(
            "wiping DB (found schema {}, need {})",
            current, kSchemaVersion
        );
        if (!execOrLog(m_db, "DROP TABLE IF EXISTS refs;"))         return false;
        if (!execOrLog(m_db, "DROP TABLE IF EXISTS commits;"))      return false;
        if (!execOrLog(m_db, "DROP TABLE IF EXISTS level_aliases;")) return false;
        if (!execOrLog(m_db, "DROP TABLE IF EXISTS schema_meta;"))  return false;
        execOrLog(m_db, "DROP INDEX IF EXISTS idx_commits_level;");
        execOrLog(m_db, "DROP INDEX IF EXISTS idx_level_aliases_canonical;");
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
    if (!execOrLog(m_db, createSchema)) return false;

    sqlite3_stmt* st = nullptr;
    constexpr char const* upsert =
        "INSERT INTO schema_meta(k, v) VALUES('version', ?) "
        "ON CONFLICT(k) DO UPDATE SET v = excluded.v;";
    if (sqlite3_prepare_v2(m_db, upsert, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare meta upsert: {}", sqlite3_errmsg(m_db));
        return false;
    }
    auto vs = std::to_string(kSchemaVersion);
    sqlite3_bind_text(st, 1, vs.c_str(), static_cast<int>(vs.size()), SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

std::optional<CommitId> CommitStore::insertAt(
    LevelKey const&         levelKey,
    std::optional<CommitId> parent,
    std::optional<CommitId> reverts,
    std::string const&      message,
    std::int64_t            createdAt,
    std::string const&      deltaBlob
) {
    if (!m_db) return std::nullopt;
    auto const canonicalKey = this->resolveOrCreateCanonicalKey(levelKey);

    constexpr char const* sql =
        "INSERT INTO commits(level_key, parent_id, reverts_id, message, created_at, delta_blob) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare insert failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }

    sqlite3_bind_text(st, 1, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT);
    if (parent)  sqlite3_bind_int64(st, 2, *parent);   else sqlite3_bind_null(st, 2);
    if (reverts) sqlite3_bind_int64(st, 3, *reverts);  else sqlite3_bind_null(st, 3);
    sqlite3_bind_text(st, 4, message.c_str(), static_cast<int>(message.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, createdAt);
    sqlite3_bind_blob(st, 6, deltaBlob.data(), static_cast<int>(deltaBlob.size()), SQLITE_TRANSIENT);

    std::optional<CommitId> out;
    if (sqlite3_step(st) == SQLITE_DONE) {
        out = sqlite3_last_insert_rowid(m_db);
    } else {
        geode::log::error("insert step failed: {}", sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(st);
    return out;
}

std::optional<CommitId> CommitStore::insert(
    LevelKey const&         levelKey,
    std::optional<CommitId> parent,
    std::optional<CommitId> reverts,
    std::string const&      message,
    std::string const&      deltaBlob
) {
    return this->insertAt(levelKey, parent, reverts, message, nowSeconds(), deltaBlob);
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
        geode::log::error("prepare get failed: {}", sqlite3_errmsg(m_db));
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
    auto const canonicalKey = this->resolveCanonicalKey(levelKey);

    constexpr char const* sql =
        "SELECT id, level_key, parent_id, reverts_id, message, created_at, delta_blob "
        "FROM commits WHERE level_key = ? "
        "ORDER BY created_at DESC, id DESC;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare list failed: {}", sqlite3_errmsg(m_db));
        return out;
    }
    sqlite3_bind_text(st, 1, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(st) == SQLITE_ROW) {
        out.push_back(rowFromStatement(st, /*includeBlob*/ true));
    }
    sqlite3_finalize(st);
    return out;
}

bool CommitStore::updateMessage(CommitId id, std::string const& message) {
    if (!m_db) return false;

    constexpr char const* sql = "UPDATE commits SET message = ? WHERE id = ?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare updateMessage failed: {}", sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(st, 1, message.c_str(), static_cast<int>(message.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, id);

    bool const ok = (sqlite3_step(st) == SQLITE_DONE) && (sqlite3_changes(m_db) > 0);
    sqlite3_finalize(st);
    return ok;
}

std::optional<CommitId> CommitStore::squash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::optional<CommitId>      parentOfOldest,
    std::string const&           message,
    std::string const&           deltaBlob
) {
    if (!m_db) return std::nullopt;
    if (idsOldestFirst.size() < 2) return std::nullopt;
    auto const canonicalKey = this->resolveCanonicalKey(levelKey);

    auto const newest = idsOldestFirst.back();

    // Inherit newest squashed commit's createdAt so the squash row keeps its
    // position in DESC-by-time list. Otherwise the squash jumps to the top
    // and rows above it become misclickable (revert hits squash instead of
    // intended newer commit).
    std::int64_t squashCreatedAt = nowSeconds();
    if (auto newestRow = this->get(newest)) {
        squashCreatedAt = newestRow->createdAt;
    }

    std::string idList;
    idList.reserve(idsOldestFirst.size() * 8);
    for (std::size_t i = 0; i < idsOldestFirst.size(); ++i) {
        if (i) idList += ',';
        idList += std::to_string(idsOldestFirst[i]);
    }

    if (!execOrLog(m_db, "BEGIN IMMEDIATE;")) return std::nullopt;
    if (!execOrLog(m_db, "PRAGMA defer_foreign_keys = ON;")) {
        execOrLog(m_db, "ROLLBACK;");
        return std::nullopt;
    }

    auto rollback = [this]() {
        execOrLog(m_db, "ROLLBACK;");
        execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
    };

    auto const newId = this->insertAt(
        canonicalKey, parentOfOldest, std::nullopt, message, squashCreatedAt, deltaBlob
    );
    if (!newId) {
        rollback();
        return std::nullopt;
    }

    auto runStmt = [&](char const* sql,
                       std::initializer_list<std::pair<int, std::int64_t>> intBinds,
                       std::initializer_list<std::pair<int, std::string const*>> textBinds) -> bool {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
            geode::log::error("prepare squash sql failed: {}", sqlite3_errmsg(m_db));
            return false;
        }
        for (auto const& b : intBinds)  sqlite3_bind_int64(st, b.first, b.second);
        for (auto const& b : textBinds) sqlite3_bind_text(st, b.first, b.second->c_str(),
                                            static_cast<int>(b.second->size()), SQLITE_TRANSIENT);
        bool ok = (sqlite3_step(st) == SQLITE_DONE);
        if (!ok) geode::log::error("squash step failed: {}", sqlite3_errmsg(m_db));
        sqlite3_finalize(st);
        return ok;
    };

    auto runSql = [this](std::string const& sql) -> bool {
        return execOrLog(m_db, sql.c_str());
    };

    if (!runStmt(
        "UPDATE commits SET parent_id = ? WHERE parent_id = ? AND level_key = ? AND id != ?;",
        {{1, *newId}, {2, newest}, {4, *newId}},
        {{3, &canonicalKey}}
    )) { rollback(); return std::nullopt; }

    if (!runSql(
        "UPDATE commits SET reverts_id = NULL WHERE reverts_id IN (" + idList + ");"
    )) { rollback(); return std::nullopt; }

    if (!runStmt(
        ("UPDATE refs SET head_id = ? WHERE head_id IN (" + idList + ") AND level_key = ?;").c_str(),
        {{1, *newId}},
        {{2, &canonicalKey}}
    )) { rollback(); return std::nullopt; }

    if (!runSql("DELETE FROM commits WHERE id IN (" + idList + ");")) {
        rollback();
        return std::nullopt;
    }

    if (!execOrLog(m_db, "COMMIT;")) {
        rollback();
        return std::nullopt;
    }
    execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
    return newId;
}

std::vector<LevelSummary> CommitStore::listLevels() {
    std::vector<LevelSummary> out;
    if (!m_db) return out;

    constexpr char const* sql =
        "SELECT level_key, COUNT(*), MAX(created_at) FROM commits "
        "GROUP BY level_key ORDER BY MAX(created_at) DESC;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare listLevels failed: {}", sqlite3_errmsg(m_db));
        return out;
    }

    while (sqlite3_step(st) == SQLITE_ROW) {
        LevelSummary s;
        auto const* key = reinterpret_cast<char const*>(sqlite3_column_text(st, 0));
        s.levelKey      = key ? key : "";
        s.commitCount   = static_cast<int>(sqlite3_column_int64(st, 1));
        s.lastCreatedAt = sqlite3_column_int64(st, 2);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(st);
    return out;
}

bool CommitStore::deleteCommitsAndRefsForKeyNoTransaction(LevelKey const& levelKey) {
    if (!m_db) return false;
    auto const canonicalKey = this->resolveCanonicalKey(levelKey);

    {
        constexpr char const* delRefs =
            "DELETE FROM refs WHERE level_key = ?;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, delRefs, -1, &st, nullptr) != SQLITE_OK) {
            geode::log::error("prepare deleteLevel refs: {}", sqlite3_errmsg(m_db));
            return false;
        }
        sqlite3_bind_text(
            st, 1, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT
        );
        if (sqlite3_step(st) != SQLITE_DONE) {
            geode::log::error("deleteLevel refs step: {}", sqlite3_errmsg(m_db));
            sqlite3_finalize(st);
            return false;
        }
        sqlite3_finalize(st);
    }

    {
        constexpr char const* delCommits =
            "DELETE FROM commits WHERE level_key = ?;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, delCommits, -1, &st, nullptr) != SQLITE_OK) {
            geode::log::error("prepare deleteLevel commits: {}", sqlite3_errmsg(m_db));
            return false;
        }
        sqlite3_bind_text(
            st, 1, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT
        );
        if (sqlite3_step(st) != SQLITE_DONE) {
            geode::log::error("deleteLevel commits step: {}", sqlite3_errmsg(m_db));
            sqlite3_finalize(st);
            return false;
        }
        sqlite3_finalize(st);
    }
    return true;
}

bool CommitStore::deleteLevel(LevelKey const& levelKey) {
    if (!m_db) return false;

    if (!execOrLog(m_db, "BEGIN IMMEDIATE;")) return false;
    if (!execOrLog(m_db, "PRAGMA defer_foreign_keys = ON;")) {
        execOrLog(m_db, "ROLLBACK;");
        return false;
    }

    if (!this->deleteCommitsAndRefsForKeyNoTransaction(levelKey)) {
        execOrLog(m_db, "ROLLBACK;");
        execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
        return false;
    }

    if (!execOrLog(m_db, "COMMIT;")) {
        execOrLog(m_db, "ROLLBACK;");
        execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
        return false;
    }
    execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
    return true;
}

bool CommitStore::replaceLevelHistoryFrom(LevelKey const& dest, LevelKey const& src) {
    if (!m_db) return false;
    auto const canonicalDest = this->resolveCanonicalKey(dest);
    auto const canonicalSrc  = this->resolveCanonicalKey(src);
    if (canonicalDest == canonicalSrc) {
        geode::log::warn("replaceLevelHistoryFrom: dest == src");
        return false;
    }

    auto const headOld = this->getHead(canonicalSrc);
    if (!headOld) {
        geode::log::error("replaceLevelHistoryFrom: no HEAD for src");
        return false;
    }

    auto const rows = this->list(canonicalSrc);
    if (rows.empty()) {
        geode::log::error("replaceLevelHistoryFrom: empty list for src");
        return false;
    }

    std::unordered_map<CommitId, CommitRow> byId;
    std::unordered_set<CommitId>            idSet;
    byId.reserve(rows.size());
    for (auto const& r : rows) {
        byId[r.id] = r;
        idSet.insert(r.id);
    }

    for (auto const& r : rows) {
        if (r.parent && !idSet.count(*r.parent)) {
            geode::log::error("replaceLevelHistoryFrom: parent {} not in level set", *r.parent);
            return false;
        }
        if (r.reverts && !idSet.count(*r.reverts)) {
            geode::log::error("replaceLevelHistoryFrom: reverts {} not in level set", *r.reverts);
            return false;
        }
    }

    std::unordered_map<CommitId, int>                    inDeg;
    std::unordered_map<CommitId, std::vector<CommitId>> afterDone;
    for (CommitId v : idSet) {
        std::unordered_set<CommitId> deps;
        auto const&                  row = byId[v];
        if (row.parent && idSet.count(*row.parent)) deps.insert(*row.parent);
        if (row.reverts && idSet.count(*row.reverts)) deps.insert(*row.reverts);
        inDeg[v] = static_cast<int>(deps.size());
        for (CommitId d : deps) {
            afterDone[d].push_back(v);
        }
    }

    std::vector<CommitId> order;
    order.reserve(idSet.size());
    std::queue<CommitId> q;
    for (CommitId id : idSet) {
        if (inDeg[id] == 0) {
            q.push(id);
        }
    }
    while (!q.empty()) {
        CommitId u = q.front();
        q.pop();
        order.push_back(u);
        for (CommitId v : afterDone[u]) {
            inDeg[v]--;
            if (inDeg[v] == 0) {
                q.push(v);
            }
        }
    }

    if (order.size() != idSet.size()) {
        geode::log::error("replaceLevelHistoryFrom: topological sort failed (cycle?)");
        return false;
    }

    if (!execOrLog(m_db, "BEGIN IMMEDIATE;")) {
        return false;
    }
    if (!execOrLog(m_db, "PRAGMA defer_foreign_keys = ON;")) {
        execOrLog(m_db, "ROLLBACK;");
        return false;
    }

    if (!this->deleteCommitsAndRefsForKeyNoTransaction(canonicalDest)) {
        execOrLog(m_db, "ROLLBACK;");
        execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
        return false;
    }

    std::unordered_map<CommitId, CommitId> idMap;
    for (CommitId const oldId : order) {
        auto const& row = byId[oldId];
        std::optional<CommitId> newParent;
        if (row.parent) {
            newParent = idMap.at(*row.parent);
        }
        std::optional<CommitId> newReverts;
        if (row.reverts) {
            newReverts = idMap.at(*row.reverts);
        }
        auto const newId = this->insertAt(
            canonicalDest, newParent, newReverts, row.message, row.createdAt, row.deltaBlob
        );
        if (!newId) {
            geode::log::error("replaceLevelHistoryFrom: insert failed at old id {}", oldId);
            execOrLog(m_db, "ROLLBACK;");
            execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
            return false;
        }
        idMap[oldId] = *newId;
    }

    if (auto it = idMap.find(*headOld); it == idMap.end()) {
        geode::log::error("replaceLevelHistoryFrom: head not in map");
        execOrLog(m_db, "ROLLBACK;");
        execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
        return false;
    } else {
        if (!this->setHead(canonicalDest, it->second)) {
            geode::log::error("replaceLevelHistoryFrom: setHead failed");
            execOrLog(m_db, "ROLLBACK;");
            execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
            return false;
        }
    }

    if (!execOrLog(m_db, "COMMIT;")) {
        execOrLog(m_db, "ROLLBACK;");
        execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
        return false;
    }
    execOrLog(m_db, "PRAGMA defer_foreign_keys = OFF;");
    return true;
}

std::optional<CommitId> CommitStore::getHead(LevelKey const& levelKey) {
    if (!m_db) return std::nullopt;
    auto const canonicalKey = this->resolveCanonicalKey(levelKey);

    constexpr char const* sql = "SELECT head_id FROM refs WHERE level_key = ?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare getHead failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }
    sqlite3_bind_text(st, 1, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT);

    std::optional<CommitId> out;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    return out;
}

bool CommitStore::setHead(LevelKey const& levelKey, CommitId head) {
    if (!m_db) return false;
    auto const canonicalKey = this->resolveOrCreateCanonicalKey(levelKey);

    constexpr char const* sql =
        "INSERT INTO refs(level_key, head_id) VALUES(?, ?) "
        "ON CONFLICT(level_key) DO UPDATE SET head_id = excluded.head_id;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare setHead failed: {}", sqlite3_errmsg(m_db));
        return false;
    }
    sqlite3_bind_text(st, 1, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, head);

    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

bool CommitStore::isLocalObservedKey(LevelKey const& levelKey) const {
    return levelKey.rfind("local:", 0) == 0;
}

bool CommitStore::upsertAlias(LevelKey const& observedKey, LevelKey const& canonicalKey) {
    if (!m_db) return false;
    sqlite3_stmt* st = nullptr;
    constexpr char const* sql =
        "INSERT INTO level_aliases(observed_key, canonical_key, created_at, last_seen_at) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT(observed_key) DO UPDATE SET "
        "canonical_key = excluded.canonical_key, "
        "last_seen_at = excluded.last_seen_at;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare upsertAlias failed: {}", sqlite3_errmsg(m_db));
        return false;
    }
    auto const now = nowSeconds();
    sqlite3_bind_text(st, 1, observedKey.c_str(), static_cast<int>(observedKey.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, now);
    sqlite3_bind_int64(st, 4, now);
    bool const ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok) {
        geode::log::error("upsertAlias step failed: {}", sqlite3_errmsg(m_db));
    }
    sqlite3_finalize(st);
    return ok;
}

std::optional<std::int64_t> CommitStore::nextCanonicalLocalId() {
    if (!m_db) return std::nullopt;
    sqlite3_stmt* st = nullptr;
    constexpr char const* sql =
        "SELECT canonical_key FROM level_aliases WHERE canonical_key LIKE 'localid:%';";
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare nextCanonicalLocalId failed: {}", sqlite3_errmsg(m_db));
        return std::nullopt;
    }
    std::int64_t maxId = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        auto const* text = reinterpret_cast<char const*>(sqlite3_column_text(st, 0));
        if (!text) continue;
        std::string_view key(text);
        if (key.rfind("localid:", 0) != 0 || key.size() <= 8) continue;
        std::int64_t value = 0;
        auto [ptr, ec] = std::from_chars(key.data() + 8, key.data() + key.size(), value);
        if (ec != std::errc() || ptr != key.data() + key.size() || value <= 0) continue;
        if (value > maxId) maxId = value;
    }
    sqlite3_finalize(st);
    return maxId + 1;
}

LevelKey CommitStore::resolveCanonicalKeyImpl(LevelKey const& observedKey, bool createIfMissing) {
    if (!m_db) return observedKey;
    if (observedKey.rfind("id:", 0) == 0) return observedKey;
    if (observedKey.rfind("localid:", 0) == 0) return observedKey;
    if (!this->isLocalObservedKey(observedKey)) return observedKey;

    sqlite3_stmt* st = nullptr;
    constexpr char const* lookup =
        "SELECT canonical_key FROM level_aliases WHERE observed_key = ?;";
    if (sqlite3_prepare_v2(m_db, lookup, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare resolveCanonicalKey lookup failed: {}", sqlite3_errmsg(m_db));
        return observedKey;
    }
    sqlite3_bind_text(st, 1, observedKey.c_str(), static_cast<int>(observedKey.size()), SQLITE_TRANSIENT);

    LevelKey canonical;
    if (sqlite3_step(st) == SQLITE_ROW) {
        auto const* text = reinterpret_cast<char const*>(sqlite3_column_text(st, 0));
        if (text) canonical = text;
    }
    sqlite3_finalize(st);

    if (!canonical.empty()) {
        if (createIfMissing) {
            this->upsertAlias(observedKey, canonical);
        }
        return canonical;
    }

    if (!createIfMissing) return observedKey;

    auto next = this->nextCanonicalLocalId();
    if (!next) return observedKey;

    canonical = "localid:" + std::to_string(*next);
    if (!this->upsertAlias(observedKey, canonical)) return observedKey;
    return canonical;
}

LevelKey CommitStore::resolveCanonicalKey(LevelKey const& observedKey) {
    return this->resolveCanonicalKeyImpl(observedKey, false);
}

LevelKey CommitStore::resolveOrCreateCanonicalKey(LevelKey const& observedKey) {
    return this->resolveCanonicalKeyImpl(observedKey, true);
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
            geode::log::error("failed to open db at {}",
                reinterpret_cast<char const*>(dbPath.u8string().c_str()));
        }
    }
    return store;
}

} // namespace git_editor
