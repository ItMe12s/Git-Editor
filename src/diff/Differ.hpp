#pragma once

#include "Delta.hpp"
#include "model/LevelState.hpp"

#include <vector>

namespace git_editor {

    struct Conflict {
        enum class Kind {
            AddAlreadyExists,
            Missing,
            ModifyStale
        };

        Kind kind;
    };

    Delta diff(LevelState const& prev, LevelState const& next);

    LevelState apply(LevelState base, Delta const& d, std::vector<Conflict>* out = nullptr);

} // namespace git_editor
