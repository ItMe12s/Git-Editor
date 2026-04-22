#pragma once

#include "CommitStore.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace git_editor {

// Portable commit representation for .gdge package files.
// commitIndex is stable within package and used for parent/reverts links.
struct GdgePackageCommit {
    std::int64_t                     commitIndex  = 0;
    std::optional<std::int64_t>      parentIndex;
    std::optional<std::int64_t>      revertsIndex;
    std::string                      message;
    std::int64_t                     createdAt    = 0;
    std::string                      deltaBlob;
};

// Portable metadata for a single-level history package.
struct GdgePackageMetadata {
    std::string                 formatVersion  = "1";
    std::string                 rootHash;
    std::optional<std::int64_t> headIndex;
    LevelKey                    sourceLevelKey;
    std::int64_t                exportedAt     = 0;
};

struct GdgePackageData {
    GdgePackageMetadata          metadata;
    std::vector<GdgePackageCommit> commits;
};

// Writes .gdge SQLite package file at outPath.
// Returns false if data is invalid or any SQLite operation fails.
bool writeGdgePackage(std::filesystem::path const& outPath, GdgePackageData const& data);

// Reads .gdge SQLite package file from path.
// Returns nullopt if file is invalid, unreadable, or missing required fields.
std::optional<GdgePackageData> readGdgePackage(std::filesystem::path const& path);

} // namespace git_editor
