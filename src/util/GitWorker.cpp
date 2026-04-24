#include "GitWorker.hpp"

#include <Geode/utils/async.hpp>

#include <memory>
#include <mutex>

namespace git_editor {

namespace {

// Serializes only postToGitWorker callbacks with each other (not main-thread DB reads).
static std::mutex s_gitWorkerMutex;

} // namespace

void postToGitWorker(std::function<void()> job) {
    auto boxed = std::make_unique<std::function<void()>>(std::move(job));
    geode::async::runtime().spawnBlocking<void>([box = std::move(boxed)]() mutable {
        std::lock_guard lock(s_gitWorkerMutex);
        if (!box || !*box) {
            return;
        }
        // Jobs are expected to be no-throw (SQLite is C API).
        (*box)();
    });
}

} // namespace git_editor
