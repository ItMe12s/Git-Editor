#pragma once

#include "util/format/Shorten.hpp"

#include <cstdint>
#include <string>

namespace git_editor {

std::string formatTimestamp(std::int64_t unixSeconds);
std::string formatBytes(std::int64_t bytes);

} // namespace git_editor
