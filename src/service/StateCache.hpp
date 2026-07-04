#pragma once

#include "model/LevelState.hpp"
#include "store/CommitStore.hpp"

#include <cstddef>
#include <unordered_map>

namespace git_editor {

class StateCache {
public:
    explicit StateCache(std::size_t cap = 64);

    void clear();
    void put(CommitId const& id, LevelStatePtr state);
    LevelStatePtr get(CommitId const& id) const;

private:
    std::size_t                                 m_cap;
    std::unordered_map<CommitId, LevelStatePtr> m_map;
};

} // namespace git_editor
