#include "Parsing.hpp"

#include <charconv>

namespace git_editor::parsing {

std::vector<std::string_view> splitView(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.emplace_back(s.data() + start, i - start);
            start = i + 1;
        }
    }
    return out;
}

bool parseInt(std::string_view s, int& out) {
    if (s.empty()) return false;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && ptr == s.data() + s.size();
}

} // namespace git_editor::parsing
