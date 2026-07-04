#pragma once

#include "model/LevelState.hpp"

namespace git_editor {

struct AssignUuidStats {
    std::size_t fingerprintHits  = 0;
    std::size_t spatialFallbacks = 0;
    std::size_t freshUuids       = 0;
};

void assignUuids(LevelState const& previous, LevelState& incoming, AssignUuidStats* stats = nullptr);

void assignFreshUuids(LevelState& state);

} // namespace git_editor
