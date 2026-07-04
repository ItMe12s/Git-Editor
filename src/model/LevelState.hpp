#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace git_editor {

using ObjectUuid = std::uint64_t;

// GD key/value pairs. Keys stay as raw strings.
// Object data uses kA* and kS* string keys alongside numeric keys.
using FieldMap = std::unordered_map<std::string, std::string>;

namespace key {
    inline const std::string kType     = "1";
    inline const std::string kX        = "2";
    inline const std::string kY        = "3";
    inline const std::string kRotation = "6";
    inline const std::string kGroups   = "57";
}

struct Object {
    ObjectUuid uuid   = 0;
    FieldMap   fields;
};

struct LevelState {
    std::string rawHeader;
    FieldMap header;
    std::unordered_map<ObjectUuid, Object> objects;
};

using LevelStatePtr = std::shared_ptr<const LevelState>;

} // namespace git_editor
