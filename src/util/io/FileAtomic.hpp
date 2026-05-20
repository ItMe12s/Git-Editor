#pragma once

#include <filesystem>

namespace git_editor {

// Atomically replace to with from.
// Windows uses MoveFileExW. POSIX uses std::filesystem::rename.
bool replaceFileAtomic(std::filesystem::path const& from, std::filesystem::path const& to);

} // namespace git_editor
