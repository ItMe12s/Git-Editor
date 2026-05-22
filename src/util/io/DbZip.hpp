#pragma once

#include "core/Result.hpp"

#include <cstdint>
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

DbFileForm peekDbFileForm(std::filesystem::path const& path);

bool writeZipAtomic(std::filesystem::path const& outZip,
                    std::string const&           entryName,
                    ByteVector const&            data);

Result<ByteVector> readZipEntry(std::filesystem::path const& inZip,
                                std::string const&           entryName);

Result<void> extractZipToFile(std::filesystem::path const& inZip,
                              std::filesystem::path const& outFile,
                              std::string const&           entryName);

} // namespace git_editor
