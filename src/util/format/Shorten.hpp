#pragma once

#include <cstddef>
#include <string>

namespace git_editor {

inline std::string shorten(std::string const& s, std::size_t maxChars) {
    if (s.size() <= maxChars) return s;
    if (maxChars <= 3) return s.substr(0, maxChars);
    return s.substr(0, maxChars - 3) + "...";
}

} // namespace git_editor
