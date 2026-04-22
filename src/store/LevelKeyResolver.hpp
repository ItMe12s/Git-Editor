#pragma once

#include "CommitStore.hpp"

struct sqlite3;

namespace git_editor::level_key_resolver {

bool isLocalObservedKey(LevelKey const& levelKey);
bool upsertAlias(sqlite3* db, LevelKey const& observedKey, LevelKey const& canonicalKey);
std::optional<std::int64_t> nextCanonicalLocalId(sqlite3* db);
LevelKey resolveCanonicalKey(sqlite3* db, LevelKey const& observedKey, bool createIfMissing);

} // namespace git_editor::level_key_resolver
