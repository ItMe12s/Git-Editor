#include "GitService.hpp"

#include "../diff/Delta.hpp"
#include "../diff/Differ.hpp"
#include "../identity/Matcher.hpp"
#include "../model/LevelParser.hpp"

#include <Geode/loader/Log.hpp>

#include <algorithm>
#include <utility>

namespace git_editor {

namespace {

std::string shortPreview(std::string s, std::size_t n = 40) {
    if (s.size() <= n) return s;
    return s.substr(0, n - 1) + "...";
}

} // namespace

GitService::GitService(CommitStore& store, std::size_t cacheCapacity)
    : m_store(store), m_cap(cacheCapacity == 0 ? 1 : cacheCapacity) {}

CommitOutcome GitService::commit(
    LevelKey const& levelKey,
    std::string const& message,
    std::string const& liveLevelStr
) {
    CommitOutcome out;

    LevelState headState;
    std::optional<CommitId> parent = m_store.getHead(levelKey);
    if (parent) {
        auto recon = this->reconstruct(*parent);
        if (!recon) {
            out.error = "failed to reconstruct HEAD; refusing to commit";
            geode::log::error("git-editor: {}", out.error);
            return out;
        }
        headState = std::move(*recon);
    }

    // Parse + assign UUIDs (matches new objects against HEAD).
    auto incoming = parseLevelString(liveLevelStr);
    if (parent) assignUuids(headState, incoming);
    else        assignFreshUuids(incoming);

    auto delta = diff(headState, incoming);

    // Empty delta is a no-op commit. We still allow it - a user may want a
    // named milestone at the current state - but we log so it's obvious.
    if (delta.adds.empty() && delta.removes.empty()
        && delta.modifies.empty() && delta.headerChanges.empty()) {
        geode::log::info("git-editor: empty commit '{}'", shortPreview(message));
    }

    auto blob = dumpDelta(delta);

    auto id = m_store.insert(levelKey, parent, std::nullopt, message, blob);
    if (!id) {
        out.error = "DB insert failed";
        geode::log::error("git-editor: {}", out.error);
        return out;
    }
    if (!m_store.setHead(levelKey, *id)) {
        out.error = "DB setHead failed (stranded commit " + std::to_string(*id) + ")";
        geode::log::error("git-editor: {}", out.error);
        return out;
    }

    this->cachePut(*id, std::move(incoming));

    out.ok = true;
    out.id = *id;
    return out;
}

CheckoutOutcome GitService::checkout(LevelKey const& levelKey, CommitId target) {
    CheckoutOutcome out;

    auto head = m_store.getHead(levelKey);
    if (!head) {
        out.error = "no HEAD for this level";
        return out;
    }
    if (*head == target) {
        // Already there. Load and return the current state, skip the commit.
        auto recon = this->reconstruct(target);
        if (!recon) { out.error = "reconstruct HEAD failed"; return out; }
        out.ok    = true;
        out.state = std::move(*recon);
        return out;
    }

    auto headState   = this->reconstruct(*head);
    auto targetState = this->reconstruct(target);
    if (!headState || !targetState) {
        out.error = "reconstruct failed";
        geode::log::error("git-editor: checkout reconstruct failed");
        return out;
    }

    auto revertDelta = diff(*headState, *targetState);
    auto blob        = dumpDelta(revertDelta);

    auto targetRow = m_store.get(target);
    std::string msg = "Checkout: " + (targetRow ? shortPreview(targetRow->message) : std::to_string(target));

    auto id = m_store.insert(levelKey, *head, target, msg, blob);
    if (!id) {
        out.error = "DB insert failed";
        return out;
    }
    if (!m_store.setHead(levelKey, *id)) {
        out.error = "DB setHead failed";
        return out;
    }

    this->cachePut(*id, *targetState);

    out.ok             = true;
    out.revertCommitId = *id;
    out.state          = std::move(*targetState);
    return out;
}

RevertOutcome GitService::revert(LevelKey const& levelKey, CommitId target) {
    RevertOutcome out;

    auto head = m_store.getHead(levelKey);
    if (!head) {
        out.error = "no HEAD for this level";
        return out;
    }

    auto targetRow = m_store.get(target);
    if (!targetRow) {
        out.error = "target commit not found";
        return out;
    }
    if (!targetRow->parent) {
        out.error = "can't revert the initial commit (it has no parent)";
        return out;
    }

    auto parentState = this->reconstruct(*targetRow->parent);
    auto targetState = this->reconstruct(target);
    auto headState   = this->reconstruct(*head);
    if (!parentState || !targetState || !headState) {
        out.error = "reconstruct failed";
        return out;
    }

    // "Undo commit C" = (target -> target.parent) delta, applied to HEAD
    // with conflict reporting. We intentionally use diff(target, parent)
    // rather than inverse(target.delta) so the op set reflects the current
    // UUIDs in `targetState` (which, if the chain has drifted, may differ
    // slightly from a literal inverse).
    auto undoDelta = diff(*targetState, *parentState);

    // Keep a copy of the pre-apply HEAD state so we can derive a
    // self-consistent persisted delta after apply() skips any conflicts.
    LevelState headCopy = *headState;
    auto newState       = apply(std::move(*headState), undoDelta, &out.conflicts);
    auto persistedDelta = diff(headCopy, newState);
    auto blob           = dumpDelta(persistedDelta);

    std::string msg = "Revert: " + shortPreview(targetRow->message);
    auto id = m_store.insert(levelKey, *head, target, msg, blob);
    if (!id) {
        out.error = "DB insert failed";
        return out;
    }
    if (!m_store.setHead(levelKey, *id)) {
        out.error = "DB setHead failed";
        return out;
    }

    this->cachePut(*id, newState);

    out.ok             = true;
    out.revertCommitId = *id;
    out.state          = std::move(newState);
    return out;
}

std::optional<LevelState> GitService::reconstruct(CommitId commitId) {
    if (auto hit = this->cacheGet(commitId)) {
        return hit;
    }

    // Walk parents to the root, collecting the commit rows in top-down
    // order (root first). Along the way, prefer cached states - any cache
    // hit short-circuits the walk since everything older is already baked
    // into the cached snapshot.
    std::vector<CommitRow> chain;
    chain.reserve(32);

    CommitId cur = commitId;
    LevelState baseState;  // starts empty; overwritten on cache hit

    while (true) {
        if (auto hit = this->cacheGet(cur)) {
            baseState = std::move(*hit);
            break;
        }
        auto row = m_store.get(cur);
        if (!row) {
            geode::log::error("git-editor: missing commit {} in chain", cur);
            return std::nullopt;
        }
        chain.push_back(*row);
        if (!row->parent) break;   // reached root
        cur = *row->parent;
    }

    // Apply deltas oldest-first. If we short-circuited on a cached ancestor
    // we skip that cached state (it's already `baseState`), otherwise we
    // start from the empty state at the root.
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto delta = parseDelta(it->deltaBlob);
        if (!delta) {
            // A corrupt delta would silently produce a wrong state and then
            // infect the LRU cache. Bail loudly so the UI can surface it.
            geode::log::error("git-editor: delta for commit {} failed to parse; "
                              "reconstruct aborted", it->id);
            return std::nullopt;
        }
        baseState = apply(std::move(baseState), *delta, /*conflicts*/ nullptr);
        this->cachePut(it->id, baseState);
    }

    return baseState;
}

void GitService::cachePut(CommitId id, LevelState state) {
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

std::optional<LevelState> GitService::cacheGet(CommitId id) {
    auto it = m_index.find(id);
    if (it == m_index.end()) return std::nullopt;
    m_lru.splice(m_lru.begin(), m_lru, it->second);
    return it->second->second;
}

GitService& sharedGitService() {
    static GitService svc(sharedCommitStore());
    return svc;
}

} // namespace git_editor
