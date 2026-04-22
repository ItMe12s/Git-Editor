#pragma once

#include "LevelState.hpp"

#include <string>
#include <string_view>

namespace git_editor {

// Parse GD 2.2081 level string, header chunk then object chunks split by the level delimiter, each chunk k,v pairs split by commas.
// Limitation: values must not contain raw commas or record delimiters (malformed tokens dropped). Key 57 uses '.' inside the value.
// Placeholder object UUIDs are 1..N by chunk index, Matcher rewrites before persist.
LevelState parseLevelString(std::string_view raw);

// Serialize, keys ascending for deterministic output.
std::string serializeLevelString(LevelState const& state);

} // namespace git_editor
