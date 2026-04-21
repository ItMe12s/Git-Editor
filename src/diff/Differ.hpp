#pragma once

#include "../model/LevelState.hpp"
#include "Delta.hpp"

#include <string>
#include <vector>

namespace git_editor {

// Reported when `apply()` finds a delta operation it can't faithfully carry
// out (object already added, target missing, or modify's expected "before"
// doesn't match the current value). The op is skipped, never forced.
struct Conflict {
    enum class Kind { AddAlreadyExists, RemoveMissing, ModifyMissing, ModifyStale };

    Kind        kind;
    ObjectUuid  uuid   = 0;
    int         field  = 0;      // only meaningful for ModifyStale
    std::string note;            // human-readable summary
};

// Produce the minimal Delta such that apply(prev, diff(prev, next)) == next.
// Assumes object UUIDs are already aligned (see Matcher).
Delta diff(LevelState const& prev, LevelState const& next);

// Swap adds<->removes and before<->after in every FieldChange (including
// entries inside `modifies`). The inverse of an inverse is the original,
// so double-invoking is always safe.
Delta inverse(Delta const& d);

// Apply `d` to a copy of `base`, skipping any conflicts. The (optional)
// `out` vector receives a report of every skipped op so callers can surface
// them to the user without having to recompute.
LevelState apply(LevelState base, Delta const& d, std::vector<Conflict>* out = nullptr);

} // namespace git_editor
