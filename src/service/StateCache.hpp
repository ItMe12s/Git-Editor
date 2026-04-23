#pragma once

#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>

namespace git_editor {

class StateCache {
public:
    explicit StateCache(std::size_t cap = 16);

    void clear();
    void put(CommitId const& id, LevelState state);
    std::optional<LevelState> get(CommitId const& id) const;

private:
    std::size_t                              m_cap;
    std::unordered_map<CommitId, LevelState> m_map;
};

} // namespace git_editor
