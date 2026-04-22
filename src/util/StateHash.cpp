#include "StateHash.hpp"

#include "../model/LevelParser.hpp"

#include <array>

namespace git_editor {

std::uint64_t fnv1a64(std::string_view text) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::string hex64(std::uint64_t value) {
    std::array<char, 16> out {};
    for (int i = 15; i >= 0; --i) {
        int nib = static_cast<int>(value & 0xFu);
        out[static_cast<std::size_t>(i)] = static_cast<char>(nib < 10 ? ('0' + nib) : ('a' + (nib - 10)));
        value >>= 4u;
    }
    return std::string(out.data(), out.size());
}

std::string hashLevelState(LevelState const& state) {
    auto normalized = serializeLevelString(state);
    return hex64(fnv1a64(normalized));
}

} // namespace git_editor
