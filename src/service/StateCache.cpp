#include "StateCache.hpp"

namespace git_editor {

StateCache::StateCache(std::size_t cap) : m_cap(cap == 0 ? 1 : cap) {}

void StateCache::clear() {
    m_map.clear();
}

void StateCache::put(CommitId const& id, LevelState state) {
    auto it = m_map.find(id);
    if (it != m_map.end()) {
        it->second = std::move(state);
        return;
    }
    if (m_map.size() >= m_cap) {
        m_map.clear();
    }
    m_map.emplace(id, std::move(state));
}

std::optional<LevelState> StateCache::get(CommitId const& id) const {
    auto it = m_map.find(id);
    if (it == m_map.end()) return std::nullopt;
    return it->second;
}

} // namespace git_editor
