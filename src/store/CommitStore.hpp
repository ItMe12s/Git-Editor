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

// deltaBlob: JSON Delta. Root commit: parent null, delta is all adds for that snapshot.
struct CommitRow {
    CommitId                 id        = 0;
    LevelKey                 levelKey;
    std::optional<CommitId>  parent;
    std::optional<CommitId>  reverts;
    std::string              message;
    std::int64_t             createdAt = 0;
    std::string              deltaBlob;
};

// SQLite, main thread. If schema_meta.version < kSchemaVersion: drop commits/refs, no migration.
class CommitStore {
public:
    static constexpr int kSchemaVersion = 2;

    CommitStore()  = default;
    ~CommitStore();

    CommitStore(CommitStore const&)            = delete;
    CommitStore& operator=(CommitStore const&) = delete;

    bool init(std::filesystem::path const& dbPath);

    std::optional<CommitId> insert(
        LevelKey const&         levelKey,
        std::optional<CommitId> parent,
        std::optional<CommitId> reverts,
        std::string const&      message,
        std::string const&      deltaBlob
    );

    std::optional<CommitRow> get(CommitId id);

    std::vector<CommitRow> list(LevelKey const& levelKey);

    std::optional<CommitId> getHead(LevelKey const& levelKey);
    bool                    setHead(LevelKey const& levelKey, CommitId head);

private:
    bool ensureSchema();

    sqlite3* m_db = nullptr;
};

CommitStore& sharedCommitStore();

} // namespace git_editor
