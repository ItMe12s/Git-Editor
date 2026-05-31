#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace git_editor {

#ifdef GIT_EDITOR_SQLITE_MAX_LENGTH
constexpr std::uint64_t kConfiguredBlobFootprintBytes = GIT_EDITOR_SQLITE_MAX_LENGTH;
#else
constexpr std::uint64_t kConfiguredBlobFootprintBytes = 1000000000u;
#endif

static_assert(kConfiguredBlobFootprintBytes <= 0xFFFFFFFFu);
constexpr std::uint32_t kMaxBlobFootprintBytes =
    static_cast<std::uint32_t>(kConfiguredBlobFootprintBytes);

bool isBlobFootprintTooLarge(std::uint64_t rawSize);
std::string blobFootprintLimitMessage(std::uint64_t rawSize);

std::optional<std::string> compressBlob(std::string const& raw);

// Returns nullopt on corrupt or oversized payload (errors logged).
std::optional<std::string> decompressBlob(std::string const& bytes);

} // namespace git_editor
