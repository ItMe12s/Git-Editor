#include "CommitStore.hpp"
#include "CommitSchema.hpp"

#include "../util/PathUtf8.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/utils/file.hpp>

#include <sqlite3.h>

#include <chrono>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace git_editor {

namespace {

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

    m_dbPath = dbPath;
    auto const utf8 = pathUtf8(dbPath);

    int rc = sqlite3_open_v2(
        utf8.c_str(), &m_db,
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
    return commit_schema::ensureSchema(m_db, kSchemaVersion);
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

std::vector<CommitSummary> CommitStore::listSummaries(LevelKey const& levelKey) {
    std::vector<CommitSummary> out;
    if (!m_db) return out;
    auto const canonicalKey = this->resolveCanonicalKey(levelKey);

    // JSON1: counts computed in SQLite so big delta_blob stays in page cache
    // instead of being copied into a std::string on the main thread.
    // `h` is an object (json_each enumerates members), `hr` is one optional field,
    // `+` `~` `-` are arrays. IFNULL/COALESCE guard missing keys.
    constexpr char const* sql =
        "SELECT id, message, created_at, "
        "(SELECT COUNT(*) FROM json_each(IFNULL(json_extract(delta_blob, '$.h'), '{}'))) + "
        "   (CASE WHEN json_type(delta_blob, '$.hr') IS NOT NULL THEN 1 ELSE 0 END) AS h_count, "
        "COALESCE(json_array_length(delta_blob, '$.\"+\"'), 0) AS add_count, "
        "COALESCE(json_array_length(delta_blob, '$.\"~\"'), 0) AS mod_count, "
        "COALESCE(json_array_length(delta_blob, '$.\"-\"'), 0) AS rm_count "
        "FROM commits WHERE level_key = ? "
        "ORDER BY created_at DESC, id DESC;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare listSummaries failed: {}", sqlite3_errmsg(m_db));
        return out;
    }
    sqlite3_bind_text(st, 1, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(st) == SQLITE_ROW) {
        CommitSummary s;
        s.id          = sqlite3_column_int64(st, 0);
        auto const* msg = reinterpret_cast<char const*>(sqlite3_column_text(st, 1));
        s.message     = msg ? msg : "";
        s.createdAt   = sqlite3_column_int64(st, 2);
        s.headerCount = sqlite3_column_int(st, 3);
        s.addCount    = sqlite3_column_int(st, 4);
        s.modifyCount = sqlite3_column_int(st, 5);
        s.removeCount = sqlite3_column_int(st, 6);
        out.push_back(std::move(s));
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
    if (ok)
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

    commit_schema::DeferredFkTransaction tx(m_db);
    if (!tx.begin()) return std::nullopt;

    auto const newId = this->insertAt(
        canonicalKey, parentOfOldest, std::nullopt, message, squashCreatedAt, deltaBlob
    );
    if (!newId) {
        tx.rollback();
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
        return commit_schema::execOrLog(m_db, sql.c_str());
    };

    if (!runStmt(
        "UPDATE commits SET parent_id = ? WHERE parent_id = ? AND level_key = ? AND id != ?;",
        {{1, *newId}, {2, newest}, {4, *newId}},
        {{3, &canonicalKey}}
    )) { tx.rollback(); return std::nullopt; }

    if (!runSql(
        "UPDATE commits SET reverts_id = NULL WHERE reverts_id IN (" + idList + ");"
    )) { tx.rollback(); return std::nullopt; }

    if (!runStmt(
        ("UPDATE refs SET head_id = ? WHERE head_id IN (" + idList + ") AND level_key = ?;").c_str(),
        {{1, *newId}},
        {{2, &canonicalKey}}
    )) { tx.rollback(); return std::nullopt; }

    if (!runSql("DELETE FROM commits WHERE id IN (" + idList + ");")) {
        tx.rollback();
        return std::nullopt;
    }
    if (!tx.commit()) return std::nullopt;

    return newId;
}

std::vector<LevelSummary> CommitStore::listLevels() {
    std::vector<LevelSummary> out;
    if (!m_db) return out;

    constexpr char const* sql =
        "SELECT level_key, COUNT(*), MAX(created_at), "
        "COALESCE(SUM(LENGTH(delta_blob)), 0) FROM commits "
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
        s.totalBytes    = sqlite3_column_int64(st, 3);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(st);
    return out;
}

bool CommitStore::deleteCommitsAndRefsForKeyNoTransaction(
    LevelKey const& levelKey
) {
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

    commit_schema::DeferredFkTransaction tx(m_db);
    if (!tx.begin()) return false;

    if (!this->deleteCommitsAndRefsForKeyNoTransaction(levelKey)) {
        tx.rollback();
        return false;
    }
    if (!tx.commit()) return false;

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

    commit_schema::DeferredFkTransaction tx(m_db);
    if (!tx.begin()) return false;

    if (!this->deleteCommitsAndRefsForKeyNoTransaction(canonicalDest)) {
        tx.rollback();
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
            tx.rollback();
            return false;
        }
        idMap[oldId] = *newId;
    }

    if (auto it = idMap.find(*headOld); it == idMap.end()) {
        geode::log::error("replaceLevelHistoryFrom: head not in map");
        tx.rollback();
        return false;
    } else {
        if (!this->setHead(canonicalDest, it->second)) {
            geode::log::error("replaceLevelHistoryFrom: setHead failed");
            tx.rollback();
            return false;
        }
    }

    if (!tx.commit()) return false;

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
    if (ok)
    sqlite3_finalize(st);
    return ok;
}

LevelKey CommitStore::resolveCanonicalKey(LevelKey const& observedKey) {
    return observedKey;
}

LevelKey CommitStore::resolveOrCreateCanonicalKey(LevelKey const& observedKey) {
    return observedKey;
}

CommitStore& sharedCommitStore() {
    static CommitStore store;
    static bool triedInit = false;
    if (!triedInit) {
        triedInit = true;
        auto* mod = geode::Mod::get();
        auto  dir = mod->getSaveDir();
        if (auto dirRes = geode::utils::file::createDirectoryAll(dir); dirRes.isErr()) {
            geode::log::error("createDirectoryAll (save dir) failed: {}", dirRes.unwrapErr());
        } else {
            auto const raw = dir / "git-editor.db";
            if (!store.init(raw)) {
                geode::log::error("failed to open db at {}", pathUtf8(raw));
            }
        }
    }
    return store;
}

} // namespace git_editor
