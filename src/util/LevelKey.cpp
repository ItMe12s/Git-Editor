#include "LevelKey.hpp"

#include <cstdint>
#include <string_view>

namespace git_editor {

namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime  = 1099511628211ull;

std::uint64_t fnv1a64(std::string_view data) {
    std::uint64_t h = kFnvOffset;
    for (unsigned char c : data) {
        h ^= static_cast<std::uint64_t>(c);
        h *= kFnvPrime;
    }
    return h;
}

std::string toHex16(std::uint64_t v) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[i] = digits[v & 0xfull];
        v >>= 4;
    }
    return out;
}

} // namespace

std::string levelKeyFor(GJGameLevel* level) {
    if (!level) {
        return "unknown:0";
    }

    // Treat any non-zero id as a "real" level id. Negative shouldn't happen
    // in practice, but we key by value rather than gating on > 0 so we don't
    // silently bucket an exotic negative id in with unsaved locals.
    int id = static_cast<int>(level->m_levelID);
    if (id != 0) {
        return "id:" + std::to_string(id);
    }

    // Unsaved / local levels: hash the name so the same level retains history
    // across sessions, while rename deliberately forks history.
    std::string_view name(level->m_levelName.c_str(), level->m_levelName.size());
    return "name:" + toHex16(fnv1a64(name));
}

} // namespace git_editor
