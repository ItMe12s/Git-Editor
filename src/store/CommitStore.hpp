#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace git_editor {

using CommitId = std::int64_t;
using LevelKey = std::string;

// A row in the commits table. `deltaBlob` is a JSON dump of a Delta struct
// (see src/diff/Delta.hpp). The first commit for a given levelKey has
// parent == std::nullopt and its delta contains only `adds` representing
// every object in the level at commit time.
struct CommitRow {
    CommitId                 id        = 0;
    LevelKey                 levelKey;
    std::optional<CommitId>  parent;
    std::optional<CommitId>  reverts;   // set for checkout + revert commits
    std::string              message;
    std::int64_t             createdAt = 0;
    std::string              deltaBlob;
};

// SQLite wrapper. Single connection, main-thread only.
//
// Schema is versioned via the `schema_meta` table. On open, if the stored
// version is missing or lower than kSchemaVersion, the commits/refs tables
// are dropped and recreated - per project decision, old snapshot-based data
// is discarded, not migrated.
class CommitStore {
public:
    static constexpr int kSchemaVersion = 2;

    CommitStore()  = default;
    ~CommitStore();

    CommitStore(CommitStore const&)            = delete;
    CommitStore& operator=(CommitStore const&) = delete;

    // Open / create the DB and ensure the current schema is installed.
    bool init(std::filesystem::path const& dbPath);

    // Append a commit. Returns the new row id on success.
    std::optional<CommitId> insert(
        LevelKey const&         levelKey,
        std::optional<CommitId> parent,
        std::optional<CommitId> reverts,
        std::string const&      message,
        std::string const&      deltaBlob
    );

    // Fetch a single commit (full row, including delta blob).
    std::optional<CommitRow> get(CommitId id);

    // List commits for `levelKey` in newest-first order. Each row carries
    // metadata only - deltaBlob is intentionally left empty to keep history
    // listings cheap. Use `get(id)` to load a specific delta.
    std::vector<CommitRow> list(LevelKey const& levelKey);

    // HEAD pointer per level (stored in `refs`).
    std::optional<CommitId> getHead(LevelKey const& levelKey);
    bool                    setHead(LevelKey const& levelKey, CommitId head);

private:
    bool ensureSchema();

    sqlite3* m_db = nullptr;
};

// Process-wide shared store, opened lazily under the mod's save dir.
CommitStore& sharedCommitStore();

} // namespace git_editor
