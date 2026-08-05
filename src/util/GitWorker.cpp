#include "GitWorker.hpp"

namespace git_editor {

    namespace detail {

        std::mutex& gitWorkerMutex() {
            static std::mutex mutex;
            return mutex;
        }

    } // namespace detail

    void postToGitWorker(std::function<void()> job) {
        spawnOnGitWorker<void>(std::move(job));
    }

} // namespace git_editor
