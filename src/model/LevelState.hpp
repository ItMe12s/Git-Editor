#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace git_editor {

// Matcher-assigned identity, not derived from fields so edits keep stable UUIDs.
using ObjectUuid = std::uint64_t;

// GD key,value pairs. Keys kept as raw strings: per-object data uses kA*/kS* string-prefixed
// keys (start-pos LSO settings, trigger sub-config) alongside numeric keys. Storing all
// keys as strings preserves them losslessly through parse/serialize/diff round-trips.
using FieldMap = std::map<std::string, std::string>;

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
    // Raw GD header chunk (before first ';'), preserved verbatim for lossless settings roundtrip.
    std::string rawHeader;
    FieldMap header;
    std::unordered_map<ObjectUuid, Object> objects;
};

} // namespace git_editor
