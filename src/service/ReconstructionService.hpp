#pragma once

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../model/LevelState.hpp"
#include "../store/CommitStore.hpp"

#include <Geode/loader/Log.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace git_editor::reconstruction_service {

template <class CacheGet, class CachePut>
std::optional<LevelState> reconstructCommitChain(
    CommitStore& store,
    CommitId commitId,
    CacheGet&& cacheGet,
    CachePut&& cachePut
) {
    if (auto hit = cacheGet(commitId)) return hit;

    std::vector<CommitRow> chain;
    chain.reserve(32);

    CommitId cur = commitId;
    LevelState baseState;

    while (true) {
        if (auto hit = cacheGet(cur)) {
            baseState = std::move(*hit);
            break;
        }
        auto row = store.get(cur);
        if (!row) {
            geode::log::error("missing commit {} in chain", cur);
            return std::nullopt;
        }
        chain.push_back(*row);
        if (!row->parent) break;
        cur = *row->parent;
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto delta = parseDelta(it->deltaBlob);
        if (!delta) {
            geode::log::error("delta for commit {} failed to parse; reconstruct aborted", it->id);
            return std::nullopt;
        }
        baseState = apply(std::move(baseState), *delta, nullptr);
        cachePut(it->id, baseState);
    }

    return baseState;
}

} // namespace git_editor::reconstruction_service
