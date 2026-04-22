#pragma once

#include "../model/LevelState.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace git_editor {

// before and after, apply uses before for conflict check and after as new value.
struct FieldChange {
    std::string before;
    std::string after;
};

// adds/removes: full objects (for inverse). modifies: uuid + changed fields only.
struct Delta {
    std::map<int, FieldChange> headerChanges;
    std::optional<FieldChange> rawHeaderChange;

    std::vector<Object> adds;
    std::vector<Object> removes;

    struct Modify {
        ObjectUuid                   uuid = 0;
        std::map<int, FieldChange>   fields;
    };
    std::vector<Modify> modifies;
};

struct DeltaStats {
    int headerChanges = 0;
    int adds          = 0;
    int modifies      = 0;
    int removes       = 0;
};

std::string dumpDelta(Delta const& d);

// Human-readable description for UI, bounded length.
std::string describeDeltaText(Delta const& d);

// std::nullopt on parse failure, callers must not substitute empty delta (corrupts state).
std::optional<Delta> parseDelta(std::string const& blob);
DeltaStats           computeStats(Delta const& d);

} // namespace git_editor
