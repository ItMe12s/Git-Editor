#pragma once

#include "../../util/AsyncQueue.hpp"

#include <Geode/Geode.hpp>

namespace git_editor::ui_action_runner {

template <class TResult, class TWorker, class TMain>
void runWorkerResult(TWorker&& worker, TMain&& onMain) {
    postToGitWorker([worker = std::forward<TWorker>(worker), onMain = std::forward<TMain>(onMain)]() mutable {
        TResult result = worker();
        geode::queueInMainThread([onMain = std::move(onMain), result = std::move(result)]() mutable {
            onMain(std::move(result));
        });
    });
}

template <class TWorker, class TMain>
void runWorkerVoid(TWorker&& worker, TMain&& onMain) {
    postToGitWorker([worker = std::forward<TWorker>(worker), onMain = std::forward<TMain>(onMain)]() mutable {
        worker();
        geode::queueInMainThread([onMain = std::move(onMain)]() mutable {
            onMain();
        });
    });
}

} // namespace git_editor::ui_action_runner
