#pragma once

#include <functional>

namespace git_editor {

// Schedules work on Geode/arc's blocking thread pool.
void postToGitWorker(std::function<void()> job);

} // namespace git_editor
