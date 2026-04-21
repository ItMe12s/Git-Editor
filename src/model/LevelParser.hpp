#pragma once

#include "LevelState.hpp"

#include <string>
#include <string_view>

namespace git_editor {

// Parse a GD 2.2081 level string into a LevelState.
//
// Format reference:
//   "<hdr-k>,<hdr-v>,...;<obj1>;<obj2>;...;"
// where each <objN> is itself comma-separated "key,value,key,value,...".
// Key 57 (groups) uses '.' internally as a sub-separator, so a bare ','
// split over an object record is safe for that key.
//
// Known limitation: this parser assumes NO object value ever contains an
// unescaped ',' or ';'. RobTop mostly honors this for 2.2081 binary /
// numeric values, but exotic text-carrying keys (some trigger payloads)
// may break it. The mod handles that gracefully - malformed key/value
// tokens are simply dropped, so the worst case is a lossy commit, not a
// crash.
//
// Placeholder UUIDs (1..N by chunk index) are filled in here; callers
// (typically Matcher::assignUuids) must rewrite them before persistence.
LevelState parseLevelString(std::string_view raw);

// Serialize a LevelState back into a GD-compatible level string. Keys are
// emitted in ascending numeric order so the output is deterministic; GD
// tolerates any key order.
std::string serializeLevelString(LevelState const& state);

} // namespace git_editor
