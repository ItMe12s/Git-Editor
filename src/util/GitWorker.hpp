#pragma once

#include <Geode/utils/async.hpp>
#include <functional>
#include <mutex>
#include <utility>

namespace git_editor {

    namespace detail {

        std::mutex& gitWorkerMutex();

    } // namespace detail

    template <class TResult, class TWorker>
    auto spawnOnGitWorker(TWorker&& worker) {
        return geode::async::runtime().spawnBlocking<TResult>(
            [worker = std::forward<TWorker>(worker)]() mutable -> TResult {
                std::lock_guard lock(detail::gitWorkerMutex());
                return worker();
            }
        );
    }

    void postToGitWorker(std::function<void()> job);

} // namespace git_editor
