#pragma once

#include <cocos2d.h>

#include <string>

namespace git_editor {

inline constexpr cocos2d::ccColor3B kAddColor  = {64, 227, 72};
inline constexpr cocos2d::ccColor3B kModColor  = {50, 200, 255};
inline constexpr cocos2d::ccColor3B kDelColor  = {255, 90, 90};
inline constexpr cocos2d::ccColor3B kHdrColor  = {255, 210, 70};
inline constexpr cocos2d::ccColor3B kTextColor = {255, 255, 255};

inline cocos2d::ccColor3B colorForDeltaLine(std::string const& line) {
    if (line.starts_with("Header")) return kHdrColor;
    if (line.starts_with("+")) return kAddColor;
    if (line.starts_with("-")) return kDelColor;
    if (line.starts_with("~") || line.starts_with("  ")) return kModColor;
    return kTextColor;
}

} // namespace git_editor
