#pragma once

#include <string>

namespace git_editor {

// Compressed: [u32 LE size][zlib]. On failure, returns input unchanged, decompress returns empty.
std::string compressBlob(std::string const& raw);

// Returns empty string on any decompression error (length prefix mismatch, zlib error).
std::string decompressBlob(std::string const& bytes);

} // namespace git_editor
