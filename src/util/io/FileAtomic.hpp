#pragma once

#include <filesystem>

namespace git_editor {

// Atomically replace `to` with `from`. Windows: MoveFileExW + REPLACE_EXISTING + WRITE_THROUGH.
// POSIX: std::filesystem::rename. Returns false and logs on failure; caller decides cleanup.
bool replaceFileAtomic(std::filesystem::path const& from, std::filesystem::path const& to);

} // namespace git_editor
