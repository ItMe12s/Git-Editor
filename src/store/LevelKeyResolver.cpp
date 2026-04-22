#include "LevelKeyResolver.hpp"

#include <Geode/loader/Log.hpp>
#include <sqlite3.h>

#include <charconv>
#include <chrono>
#include <string_view>
#include <unordered_set>

namespace git_editor::level_key_resolver {

namespace {

std::int64_t nowSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace

bool isLocalObservedKey(LevelKey const& levelKey) {
    return levelKey.rfind("local:", 0) == 0;
}

bool upsertAlias(sqlite3* db, LevelKey const& observedKey, LevelKey const& canonicalKey) {
    if (!db) return false;
    sqlite3_stmt* st = nullptr;
    constexpr char const* sql =
        "INSERT INTO level_aliases(observed_key, canonical_key, created_at, last_seen_at) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT(observed_key) DO UPDATE SET "
        "canonical_key = excluded.canonical_key, "
        "last_seen_at = excluded.last_seen_at;";
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare upsertAlias failed: {}", sqlite3_errmsg(db));
        return false;
    }
    auto const now = nowSeconds();
    sqlite3_bind_text(st, 1, observedKey.c_str(), static_cast<int>(observedKey.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, canonicalKey.c_str(), static_cast<int>(canonicalKey.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, now);
    sqlite3_bind_int64(st, 4, now);
    bool const ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok) {
        geode::log::error("upsertAlias step failed: {}", sqlite3_errmsg(db));
    }
    sqlite3_finalize(st);
    return ok;
}

std::optional<std::int64_t> nextCanonicalLocalId(sqlite3* db) {
    if (!db) return std::nullopt;
    sqlite3_stmt* st = nullptr;
    constexpr char const* sql =
        "SELECT canonical_key FROM level_aliases WHERE canonical_key LIKE 'localid:%';";
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare nextCanonicalLocalId failed: {}", sqlite3_errmsg(db));
        return std::nullopt;
    }
    std::unordered_set<std::int64_t> usedIds;
    int rc = SQLITE_ROW;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        auto const* text = reinterpret_cast<char const*>(sqlite3_column_text(st, 0));
        if (!text) continue;
        std::string_view key(text);
        if (key.rfind("localid:", 0) != 0 || key.size() <= 8) continue;
        std::int64_t value = 0;
        auto [ptr, ec] = std::from_chars(key.data() + 8, key.data() + key.size(), value);
        if (ec != std::errc() || ptr != key.data() + key.size() || value <= 0) continue;
        usedIds.insert(value);
    }
    if (rc != SQLITE_DONE) {
        geode::log::error("nextCanonicalLocalId step failed: {}", sqlite3_errmsg(db));
        sqlite3_finalize(st);
        return std::nullopt;
    }
    sqlite3_finalize(st);

    std::int64_t candidate = 1;
    while (usedIds.contains(candidate)) {
        ++candidate;
    }
    return candidate;
}

LevelKey resolveCanonicalKey(sqlite3* db, LevelKey const& observedKey, bool createIfMissing) {
    if (!db) return observedKey;
    if (observedKey.rfind("id:", 0) == 0) return observedKey;
    if (observedKey.rfind("localid:", 0) == 0) return observedKey;
    if (!isLocalObservedKey(observedKey)) return observedKey;

    sqlite3_stmt* st = nullptr;
    constexpr char const* lookup =
        "SELECT canonical_key FROM level_aliases WHERE observed_key = ?;";
    if (sqlite3_prepare_v2(db, lookup, -1, &st, nullptr) != SQLITE_OK) {
        geode::log::error("prepare resolveCanonicalKey lookup failed: {}", sqlite3_errmsg(db));
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
            upsertAlias(db, observedKey, canonical);
        }
        return canonical;
    }

    if (!createIfMissing) return observedKey;

    auto next = nextCanonicalLocalId(db);
    if (!next) return observedKey;

    canonical = "localid:" + std::to_string(*next);
    if (!upsertAlias(db, observedKey, canonical)) return observedKey;
    return canonical;
}

} // namespace git_editor::level_key_resolver
