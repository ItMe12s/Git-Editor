#include "FileAtomic.hpp"

#include <Geode/loader/Log.hpp>

#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace git_editor {

bool replaceFileAtomic(std::filesystem::path const& from, std::filesystem::path const& to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        geode::log::error("replaceFileAtomic: MoveFileExW failed: {}", GetLastError());
        return false;
    }
    return true;
#else
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec) {
        geode::log::error("replaceFileAtomic: rename failed: {}", ec.message());
        return false;
    }
    return true;
#endif
}

} // namespace git_editor
