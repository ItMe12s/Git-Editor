#pragma once

#include "../model/LevelState.hpp"

namespace git_editor {

// Align incoming UUIDs to previous: fingerprint FIFO, then same-type spatial within threshold, else new UUID.
void assignUuids(LevelState const& previous, LevelState& incoming);

void assignFreshUuids(LevelState& state);

} // namespace git_editor
