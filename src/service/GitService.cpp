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
            out.error = "failed to reconstruct HEAD, refusing to commit";
            geode::log::error("{}", out.error);
            return out;
        }
        headState = std::move(*recon);
    }

    auto incoming = parseLevelString(liveLevelStr);
    if (parent) assignUuids(headState, incoming);
    else        assignFreshUuids(incoming);

    auto delta = diff(headState, incoming);

    if (delta.adds.empty() && delta.removes.empty()
        && delta.modifies.empty() && delta.headerChanges.empty()) {
        geode::log::info("empty commit '{}'", shortPreview(message));
    }

    auto blob = dumpDelta(delta);

    auto id = m_store.insert(levelKey, parent, std::nullopt, message, blob);
    if (!id) {
        out.error = "DB insert failed";
        geode::log::error("{}", out.error);
        return out;
    }
    if (!m_store.setHead(levelKey, *id)) {
        out.error = "DB setHead failed (stranded commit " + std::to_string(*id) + ")";
        geode::log::error("{}", out.error);
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
        geode::log::error("checkout reconstruct failed");
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

    // diff(target, parent) not inverse(stored delta): ops use current UUIDs if chain drifted.
    auto undoDelta = diff(*targetState, *parentState);

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

SquashOutcome GitService::squash(
    LevelKey const&              levelKey,
    std::vector<CommitId> const& idsOldestFirst,
    std::string const&           message
) {
    SquashOutcome out;

    if (idsOldestFirst.size() < 2) {
        out.error = "Squash needs at least 2 commits";
        return out;
    }

    std::vector<CommitRow> rows;
    rows.reserve(idsOldestFirst.size());
    for (auto id : idsOldestFirst) {
        auto row = m_store.get(id);
        if (!row) {
            out.error = "Commit " + std::to_string(id) + " not found";
            return out;
        }
        if (row->levelKey != levelKey) {
            out.error = "Commit " + std::to_string(id) + " belongs to a different level";
            return out;
        }
        rows.push_back(std::move(*row));
    }

    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (!rows[i].parent || *rows[i].parent != rows[i - 1].id) {
            out.error = "Selected commits are not contiguous";
            return out;
        }
    }

    auto const parentOfOldest = rows.front().parent;

    LevelState base;
    if (parentOfOldest) {
        auto recon = this->reconstruct(*parentOfOldest);
        if (!recon) { out.error = "reconstruct base failed"; return out; }
        base = std::move(*recon);
    }

    auto target = this->reconstruct(rows.back().id);
    if (!target) { out.error = "reconstruct target failed"; return out; }

    auto combined = diff(base, *target);
    auto blob     = dumpDelta(combined);

    auto newId = m_store.squash(levelKey, idsOldestFirst, parentOfOldest, message, blob);
    if (!newId) {
        out.error = "DB squash failed";
        return out;
    }

    this->clearReconstructCache();

    out.ok          = true;
    out.newCommitId = *newId;
    out.state       = std::move(*target);
    return out;
}

ImportLevelOutcome GitService::importLevelFrom(LevelKey const& dest, LevelKey const& src) {
    ImportLevelOutcome out;
    if (dest == src) {
        out.error = "source and destination are the same";
        return out;
    }
    if (!m_store.replaceLevelHistoryFrom(dest, src)) {
        out.error = "failed to copy level history";
        return out;
    }
    this->clearReconstructCache();
    auto const head = m_store.getHead(dest);
    if (!head) {
        out.error = "no HEAD after import";
        return out;
    }
    auto st = this->reconstruct(*head);
    if (!st) {
        out.error = "reconstruct after import failed";
        return out;
    }
    out.ok    = true;
    out.state = std::move(*st);
    return out;
}

void GitService::clearReconstructCache() {
    m_lru.clear();
    m_index.clear();
}

std::optional<LevelState> GitService::reconstruct(CommitId commitId) {
    if (auto hit = this->cacheGet(commitId)) {
        return hit;
    }

    // Collect chain root to tip, cache hit on ancestor ends walk early.
    std::vector<CommitRow> chain;
    chain.reserve(32);

    CommitId cur = commitId;
    LevelState baseState;

    while (true) {
        if (auto hit = this->cacheGet(cur)) {
            baseState = std::move(*hit);
            break;
        }
        auto row = m_store.get(cur);
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
            geode::log::error("delta for commit {} failed to parse; "
                              "reconstruct aborted", it->id);
            return std::nullopt;
        }
        baseState = apply(std::move(baseState), *delta, nullptr);
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
