#pragma once

#include "../model/LevelState.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace git_editor {

// Before/after value pair. Applied literally during inverse; `apply` uses
// `before` to detect conflicts (3-way style) and `after` as the new value.
struct FieldChange {
    std::string before;
    std::string after;
};

// One pure delta between two LevelStates. Inverse + apply are defined over
// this type (see Differ.hpp).
//
// Invariants:
//   - `adds`     contains full objects (uuid + every field) so an inverse
//                has enough data to reconstruct them as a "remove".
//   - `removes`  contains full objects for the same reason.
//   - `modifies` references an existing UUID and lists ONLY changed fields.
struct Delta {
    std::map<int, FieldChange> headerChanges;

    std::vector<Object> adds;
    std::vector<Object> removes;

    struct Modify {
        ObjectUuid                   uuid = 0;
        std::map<int, FieldChange>   fields;
    };
    std::vector<Modify> modifies;
};

// Serialization is plain JSON via matjson. Kept as a free-function pair
// (rather than matjson::Serialize<Delta>) because Delta transitively owns
// FieldMap/Object which have their own well-defined on-disk shape here.
std::string dumpDelta(Delta const& d);

// Returns std::nullopt (and logs via geode::log::error) on parse failure.
// Callers MUST treat a failed parse as fatal for the operation - silently
// substituting an empty Delta would corrupt reconstructed state.
std::optional<Delta> parseDelta(std::string const& blob);

} // namespace git_editor
