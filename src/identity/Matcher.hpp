#pragma once

#include "../model/LevelState.hpp"

namespace git_editor {

// Rewrites `incoming`'s object UUIDs to reuse the UUIDs from `previous`
// wherever a plausible identity match exists. Objects that can't be matched
// receive freshly-minted random 64-bit UUIDs.
//
// Matching passes, in order:
//   1. Fingerprint bucket: (type, round(x), round(y), rotation, groups).
//   2. Spatial nearest-neighbor among remaining prev objects of the same
//      type, within kSpatialThreshold units.
//   3. Fresh UUID.
//
// After this call, `incoming` has stable UUIDs ready for diff/persist.
void assignUuids(LevelState const& previous, LevelState& incoming);

// Convenience used on the very first commit for a level: every object gets
// a fresh UUID, no matching needed.
void assignFreshUuids(LevelState& state);

} // namespace git_editor
