#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace git_editor {

constexpr std::uint32_t kMaxBlobFootprintBytes = 16u * 1024u * 1024u;

std::optional<std::string> compressBlob(std::string const& raw);

// Returns nullopt on corrupt or oversized payload (errors logged).
std::optional<std::string> decompressBlob(std::string const& bytes);

} // namespace git_editor
