#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace git_editor {

// Synthetic per-object identity, assigned by Matcher the first time we see
// an object. Never derived from the object's own fields, so an object can be
// moved / restyled / regrouped and still keep its UUID across commits.
using ObjectUuid = std::uint64_t;

// GD serializes each object as comma-separated "key,value" pairs where keys
// are small positive integers. We preserve values as their raw textual form
// so roundtripping never loses precision or alters normalization.
using FieldMap = std::map<int, std::string>;

// Well-known GD object keys we rely on for identity matching / rendering.
namespace key {
    constexpr int kType     = 1;   // object id (GDObject type enum)
    constexpr int kX        = 2;
    constexpr int kY        = 3;
    constexpr int kRotation = 6;
    constexpr int kGroups   = 57;  // '.'-separated list inside the one value
}

struct Object {
    ObjectUuid uuid   = 0;
    FieldMap   fields;
};

struct LevelState {
    // Header (everything before the first object in the level string).
    FieldMap header;

    // All level objects, keyed by UUID so diff / apply can do O(1) lookups.
    std::unordered_map<ObjectUuid, Object> objects;
};

} // namespace git_editor
