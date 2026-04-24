#pragma once

#include <Geode/utils/string.hpp>

#include <filesystem>
#include <string>

namespace git_editor {

// UTF-8 string for display and APIs that take narrow UTF-8 (e.g. sqlite3_open on non-Win).
inline std::string pathUtf8(std::filesystem::path const& path) {
    return geode::utils::string::pathToString(path);
}

} // namespace git_editor
