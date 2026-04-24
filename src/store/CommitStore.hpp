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
struct LevelSummary {
    LevelKey     levelKey;
    int          commitCount   = 0;
    std::int64_t lastCreatedAt = 0;
};

struct CommitRow {
    CommitId                 id        = 0;
    LevelKey                 levelKey;
    std::optional<CommitId>  parent;
    std::optional<CommitId>  reverts;
    std::string              message;
    std::int64_t             createdAt = 0;
    std::string              deltaBlob;
};

// Blob-free row for list UI.
struct CommitSummary {
    CommitId     id          = 0;
    std::string  message;
    std::int64_t createdAt   = 0;
    int          headerCount = 0;
    int          addCount    = 0;
    int          modifyCount = 0;
    int          removeCount = 0;
};

// One sqlite3* per process (sharedCommitStore). The UI may read on the main thread, mutating
// operations are usually scheduled on Geode's async blocking pool via postToGitWorker. The worker
// mutex serializes only jobs posted to postToGitWorker, not all store access. If
// schema_meta.version < kSchemaVersion:
// drop commits/refs, no migration.
class CommitStore {
public:
    static constexpr int kSchemaVersion = 4;

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

    std::vector<CommitRow>     list(LevelKey const& levelKey);
    std::vector<CommitSummary> listSummaries(LevelKey const& levelKey);
    bool                       updateMessage(CommitId id, std::string const& message);

    // Atomically replaces a contiguous range [oldest..newest] (oldest-first ids) with one new
    // commit. Re-parents children of newest, moves HEAD if it pointed at any squashed commit,
    // clears reverts pointers into the squashed set. Returns new commit id or nullopt.
    std::optional<CommitId> squash(
        LevelKey const&              levelKey,
        std::vector<CommitId> const& idsOldestFirst,
        std::optional<CommitId>      parentOfOldest,
        std::string const&           message,
        std::string const&           deltaBlob
    );

    std::vector<LevelSummary> listLevels();
    bool                      deleteLevel(LevelKey const& levelKey);

    // Wipes dest's commits/refs, deep-copies all commits for src to dest (new ids, remapped
    // parent_id/reverts_id). Preserves created_at, message, delta. Sets HEAD to copy of src HEAD.
    // Single transaction. Returns false on failure or if dest == src.
    bool replaceLevelHistoryFrom(LevelKey const& dest, LevelKey const& src);

    std::optional<CommitId> getHead(LevelKey const& levelKey);
    bool                    setHead(LevelKey const& levelKey, CommitId head);

    LevelKey resolveCanonicalKey(LevelKey const& observedKey);
    LevelKey resolveOrCreateCanonicalKey(LevelKey const& observedKey);

private:
    bool ensureSchema();

    std::optional<CommitId> insertAt(
        LevelKey const&         levelKey,
        std::optional<CommitId> parent,
        std::optional<CommitId> reverts,
        std::string const&      message,
        std::int64_t            createdAt,
        std::string const&      deltaBlob
    );

    bool deleteCommitsAndRefsForKeyNoTransaction(LevelKey const& levelKey);

    sqlite3*              m_db    = nullptr;
};

CommitStore& sharedCommitStore();

} // namespace git_editor
