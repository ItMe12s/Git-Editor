#pragma once

#include <functional>

namespace git_editor {

void postToGitWorker(std::function<void()> job);

} // namespace git_editor
