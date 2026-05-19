#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace git_editor {

// Hard cap on raw (uncompressed) blob size. Applied both ways: compress refuses larger inputs,
// decompress refuses headers that claim larger sizes. Shared with DbZip extract cap.
constexpr std::uint32_t kMaxBlobFootprintBytes = 16u * 1024u * 1024u;

// Compressed format: [u32 LE size][zlib]. Returns nullopt on oversize input or zlib failure
// (never falls back to raw, never silently stores bare JSON).
std::optional<std::string> compressBlob(std::string const& raw);

// Returns empty string on any decompression error (length prefix mismatch, oversize, zlib error).
std::string decompressBlob(std::string const& bytes);

} // namespace git_editor
