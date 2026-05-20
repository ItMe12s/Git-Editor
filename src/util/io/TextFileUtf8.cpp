#include "TextFileUtf8.hpp"

#include <fstream>

namespace git_editor {

bool writeTextFileUtf8(std::filesystem::path const& path, std::string const& utf8) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return static_cast<bool>(out);
}

} // namespace git_editor
