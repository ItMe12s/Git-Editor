#pragma once

#include <Geode/utils/string.hpp>

#include <filesystem>
#include <string>

namespace git_editor {

// UTF-8 path string via Geode (logging, sqlite3_open filename argument, user-visible paths).
inline std::string pathUtf8(std::filesystem::path const& path) {
    return geode::utils::string::pathToString(path);
}

} // namespace git_editor
