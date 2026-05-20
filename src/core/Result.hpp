#pragma once

// When ok is false, value is not meaningful. read it only if ok is true.
#include <string>

namespace git_editor {

template <typename T>
struct Result {
    bool        ok = false;
    T           value{};
    std::string error;
};

template <>
struct Result<void> {
    bool        ok = false;
    std::string error;
};

} // namespace git_editor
