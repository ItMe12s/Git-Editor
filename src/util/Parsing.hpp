#pragma once
#include <string_view>
#include <vector>

namespace git_editor::parsing {
std::vector<std::string_view> splitView(std::string_view s, char delim);
bool parseInt(std::string_view s, int& out);
} // namespace git_editor::parsing
