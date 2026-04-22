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

    std::vector<Object> adds;
    std::vector<Object> removes;

    struct Modify {
        ObjectUuid                   uuid = 0;
        std::map<int, FieldChange>   fields;
    };
    std::vector<Modify> modifies;
};

std::string dumpDelta(Delta const& d);

// std::nullopt on parse failure, callers must not substitute empty delta (corrupts state).
std::optional<Delta> parseDelta(std::string const& blob);

} // namespace git_editor
