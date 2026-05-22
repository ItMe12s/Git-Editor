#pragma once

#include <Geode/utils/string.hpp>

#include <filesystem>
#include <string>

namespace git_editor {

inline std::string pathUtf8(std::filesystem::path const& path) {
    // This wrapper is here because we used to have something worse.
    return geode::utils::string::pathToString(path);
}

} // namespace git_editor
