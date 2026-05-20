#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace git_editor {

std::string formatTimestamp(std::int64_t unixSeconds);
std::string shorten(std::string const& s, std::size_t maxChars);
std::string formatBytes(std::int64_t bytes);

} // namespace git_editor
