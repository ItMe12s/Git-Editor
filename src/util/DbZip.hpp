#pragma once

#include "../service/Result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace git_editor {

using ByteVector = std::vector<std::uint8_t>;

enum class DbFileForm {
    Sqlite,
    Zip,
    Unknown,
};

// Peek first bytes to determine file format. Does not open DB.
DbFileForm peekDbFileForm(std::filesystem::path const& path);

// Write data as a single zip entry at outZip.tmp, then atomically rename to outZip.
// entryName is the filename stored inside the zip.
bool writeZipAtomic(std::filesystem::path const& outZip,
                    std::string const&            entryName,
                    ByteVector const&             data);

// Read the first entry from a zip file matching entryName (or the sole entry if name is
// empty). Returns bytes on success, error on failure.
Result<ByteVector> readZipEntry(std::filesystem::path const& inZip,
                                std::string const&            entryName);

// Extract a single zip entry to outFile on disk. Convenience wrapper used at startup.
bool extractZipToFile(std::filesystem::path const& inZip,
                      std::filesystem::path const& outFile,
                      std::string const&            entryName);

} // namespace git_editor
