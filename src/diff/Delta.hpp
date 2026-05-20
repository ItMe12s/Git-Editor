#pragma once

#include "model/LevelState.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace git_editor {

struct FieldChange {
    std::string before;
    std::string after;
};

struct Delta {
    std::map<std::string, FieldChange> headerChanges;
    std::optional<FieldChange>         rawHeaderChange;

    std::vector<Object> adds;
    std::vector<Object> removes;

    struct Modify {
        ObjectUuid                            uuid = 0;
        std::map<std::string, FieldChange>    fields;
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

std::optional<Delta> parseDelta(std::string const& blob);
DeltaStats           computeStats(Delta const& d);

} // namespace git_editor
