#pragma once

#include "../model/LevelState.hpp"
#include "Delta.hpp"

#include <string>
#include <vector>

namespace git_editor {

struct Conflict {
    enum class Kind { AddAlreadyExists, RemoveMissing, ModifyMissing, ModifyStale };

    Kind        kind;
    ObjectUuid  uuid   = 0;
    int         field  = 0;
    std::string note;
};

Delta diff(LevelState const& prev, LevelState const& next);

Delta inverse(Delta const& d);

LevelState apply(LevelState base, Delta const& d, std::vector<Conflict>* out = nullptr);

} // namespace git_editor
