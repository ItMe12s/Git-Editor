#include "GitWorker.hpp"

#include <Geode/utils/async.hpp>

#include <memory>
#include <mutex>

namespace git_editor {

namespace {

std::mutex gitWorkerMutex;

} // namespace

void postToGitWorker(std::function<void()> job) {
    auto boxed = std::make_unique<std::function<void()>>(std::move(job));
    geode::async::runtime().spawnBlocking<void>([box = std::move(boxed)]() mutable {
        std::lock_guard lock(gitWorkerMutex);
        (*box)();
    });
}

} // namespace git_editor
