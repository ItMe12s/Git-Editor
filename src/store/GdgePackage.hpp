#pragma once

#include "CommitStore.hpp"
#include "model/LevelState.hpp"
#include "core/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace git_editor {

struct GdgePackageCommit {
    std::int64_t                     commitIndex  = 0;
    std::optional<std::int64_t>      parentIndex;
    std::optional<std::int64_t>      revertsIndex;
    std::string                      message;
    std::int64_t                     createdAt    = 0;
    std::string                      deltaBlob;
};

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

Result<void> writeGdgePackage(std::filesystem::path const& outPath, GdgePackageData const& data);

Result<GdgePackageData> readGdgePackage(std::filesystem::path const& path);

} // namespace git_editor
