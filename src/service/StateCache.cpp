#include "StateCache.hpp"

namespace git_editor {

StateCache::StateCache(std::size_t capacity) : m_cap(capacity == 0 ? 1 : capacity) {}

void StateCache::clear() {
    m_lru.clear();
    m_index.clear();
}

void StateCache::put(CommitId id, LevelState state) {
    auto it = m_index.find(id);
    if (it != m_index.end()) {
        it->second->second = std::move(state);
        m_lru.splice(m_lru.begin(), m_lru, it->second);
        return;
    }

    m_lru.emplace_front(id, std::move(state));
    m_index[id] = m_lru.begin();

    while (m_lru.size() > m_cap) {
        auto& victim = m_lru.back();
        m_index.erase(victim.first);
        m_lru.pop_back();
    }
}

std::optional<LevelState> StateCache::get(CommitId id) {
    auto it = m_index.find(id);
    if (it == m_index.end()) return std::nullopt;
    m_lru.splice(m_lru.begin(), m_lru, it->second);
    return it->second->second;
}

} // namespace git_editor
