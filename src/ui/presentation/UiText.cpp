#include "UiText.hpp"

#include <ctime>
#include <fmt/format.h>

namespace git_editor {

std::string formatTimestamp(std::int64_t unixSeconds) {
    std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm tm{};
#if defined(_WIN32)
    if (localtime_s(&tm, &t) != 0) return "?";
#else
    if (localtime_r(&t, &tm) == nullptr) return "?";
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm) == 0) return "?";
    return std::string(buf);
}

std::string shorten(std::string const& s, std::size_t maxChars) {
    if (s.size() <= maxChars) return s;
    if (maxChars <= 3) return s.substr(0, maxChars);
    return s.substr(0, maxChars - 3) + "...";
}

std::string formatBytes(std::int64_t bytes) {
    if (bytes < 1024) return fmt::format("{} B", bytes);
    double const kb = bytes / 1024.0;
    if (kb < 1024.0) return fmt::format("{:.0f} KB", kb);
    double const mb = kb / 1024.0;
    if (mb < 1024.0) return fmt::format("{:.1f} MB", mb);
    double const gb = mb / 1024.0;
    return fmt::format("{:.2f} GB", gb);
}

} // namespace git_editor
