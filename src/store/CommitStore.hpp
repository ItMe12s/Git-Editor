#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace git_editor {

using CommitId = std::int64_t;
using LevelKey = std::string;

struct LevelSummary {
    LevelKey     levelKey;
    int          commitCount   = 0;
    std::int64_t lastCreatedAt = 0;
    std::int64_t totalBytes    = 0;
};

struct CommitRow {
    CommitId                 id        = 0;
    LevelKey                 levelKey;
    std::optional<CommitId>  parent;
    std::optional<CommitId>  reverts;
    std::string              message;
    std::int64_t             createdAt = 0;
    // Decompressed delta JSON from get()/list(). Use parseDelta directly.
    std::string              deltaBlob;
};

struct CommitSummary {
    CommitId     id          = 0;
    std::string  message;
    std::int64_t createdAt   = 0;
    int          headerCount = 0;
    int          addCount    = 0;
    int          modifyCount = 0;
    int          removeCount = 0;
};

struct CommitSummaryRow {
    CommitId     id        = 0;
    std::string  message;
    std::int64_t createdAt = 0;
    // Compressed blob as stored in SQLite. decompressBlob before parseDelta (nullopt if corrupt).
    std::string  deltaBlob;
};

class CommitStore {
public:
    static constexpr int kSchemaVersion = 5;

    CommitStore()  = default;
    ~CommitStore();

    CommitStore(CommitStore const&)            = delete;
    CommitStore& operator=(CommitStore const&) = delete;

    bool init(std::filesystem::path const& dbPath);

    std::filesystem::path const& dbPath() const { return m_dbPath; }

    // deltaBlob: uncompressed delta JSON, stored compressed in SQLite.
    std::optional<CommitId> insertAndSetHead(
        LevelKey const&         levelKey,
        std::optional<CommitId> parent,
        std::optional<CommitId> reverts,
        std::string const&      message,
        std::string const&      deltaBlob
    );

    std::optional<CommitRow> get(CommitId id);

    std::vector<CommitRow>     list(LevelKey const& levelKey);
    std::vector<CommitSummaryRow> listSummaryRows(LevelKey const& levelKey);
    bool                       updateMessage(CommitId id, std::string const& message);

    std::optional<CommitId> squash(
        LevelKey const&              levelKey,
        std::vector<CommitId> const& idsOldestFirst,
        std::optional<CommitId>      parentOfOldest,
        std::string const&           message,
        std::string const&           deltaBlob
    );

    std::vector<LevelSummary> listLevels();
    bool                      deleteLevel(LevelKey const& levelKey);

    bool replaceLevelHistoryFrom(LevelKey const& dest, LevelKey const& src);

    std::optional<CommitId> getHead(LevelKey const& levelKey);
    bool                    setHead(LevelKey const& levelKey, CommitId head);

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

    bool prepareStatements();
    void finalizeStatements();

    static void resetStatement(sqlite3_stmt* st);

    mutable std::recursive_mutex m_mutex;
    sqlite3*              m_db        = nullptr;
    std::filesystem::path m_dbPath;

    sqlite3_stmt* m_stmtInsert        = nullptr;
    sqlite3_stmt* m_stmtGet           = nullptr;
    sqlite3_stmt* m_stmtList          = nullptr;
    sqlite3_stmt* m_stmtListSummaries = nullptr;
    sqlite3_stmt* m_stmtUpdateMessage = nullptr;
    sqlite3_stmt* m_stmtListLevels    = nullptr;
    sqlite3_stmt* m_stmtDelRefs       = nullptr;
    sqlite3_stmt* m_stmtDelCommits    = nullptr;
    sqlite3_stmt* m_stmtGetHead       = nullptr;
    sqlite3_stmt* m_stmtSetHead       = nullptr;
};

CommitStore& sharedCommitStore();

} // namespace git_editor
