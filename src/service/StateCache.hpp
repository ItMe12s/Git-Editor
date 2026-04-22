#pragma once

#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"

#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace git_editor {

class StateCache {
public:
    explicit StateCache(std::size_t capacity = 16);

    void clear();
    void put(CommitId id, LevelState state);
    std::optional<LevelState> get(CommitId id);

private:
    std::size_t m_cap;
    std::list<std::pair<CommitId, LevelState>> m_lru;
    std::unordered_map<CommitId, std::list<std::pair<CommitId, LevelState>>::iterator> m_index;
};

} // namespace git_editor
