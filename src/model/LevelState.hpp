#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace git_editor {

// Matcher-assigned identity, not derived from fields so edits keep stable UUIDs.
using ObjectUuid = std::uint64_t;

// GD key,value pairs, values kept as raw strings for lossless roundtrip.
using FieldMap = std::map<int, std::string>;

namespace key {
    constexpr int kType     = 1;
    constexpr int kX        = 2;
    constexpr int kY        = 3;
    constexpr int kRotation = 6;
    constexpr int kGroups   = 57;
}

struct Object {
    ObjectUuid uuid   = 0;
    FieldMap   fields;
};

struct LevelState {
    FieldMap header;
    std::unordered_map<ObjectUuid, Object> objects;
};

} // namespace git_editor
