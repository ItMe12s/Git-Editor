#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace git_editor {

struct Commit {
    std::int64_t id        = 0;
    std::string  levelKey;
    std::string  message;
    std::int64_t createdAt = 0; // unix seconds
    // levelString is only populated by getCommit(); listCommits() leaves it empty
    // to avoid loading every level blob into memory for the history list.
    std::string  levelString;
};

// Thin SQLite wrapper. Single DB file, single connection, single thread
// (all calls are expected from the main editor thread).
class CommitStore {
public:
    CommitStore()  = default;
    ~CommitStore();

    CommitStore(CommitStore const&)            = delete;
    CommitStore& operator=(CommitStore const&) = delete;

    // Open (or create) the database at `dbPath` and ensure schema exists.
    // Returns true on success.
    bool init(std::filesystem::path const& dbPath);

    // Insert a new commit. Returns the row id on success, std::nullopt on failure.
    std::optional<std::int64_t> addCommit(
        std::string const& levelKey,
        std::string const& message,
        std::string const& levelString
    );

    // Return commits for `levelKey`, newest first. levelString is NOT loaded
    // (use getCommit for that).
    std::vector<Commit> listCommits(std::string const& levelKey);

    // Fetch a single commit, including levelString. std::nullopt if not found.
    std::optional<Commit> getCommit(std::int64_t id);

private:
    sqlite3* m_db = nullptr;
};

// Process-wide shared store, opened lazily under the mod's save dir.
CommitStore& sharedCommitStore();

} // namespace git_editor
