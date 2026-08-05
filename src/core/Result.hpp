#pragma once

#include <expected>
#include <string>

namespace git_editor {

template <typename T>
using Result = std::expected<T, std::string>;

} // namespace git_editor
