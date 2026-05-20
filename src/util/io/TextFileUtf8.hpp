#pragma once

#include <filesystem>
#include <string>

namespace git_editor {

bool writeTextFileUtf8(std::filesystem::path const& path, std::string const& utf8);

} // namespace git_editor
