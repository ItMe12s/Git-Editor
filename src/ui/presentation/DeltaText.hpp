#pragma once

#include "../../diff/Delta.hpp"

#include <string>

namespace git_editor {

std::string describeDeltaText(Delta const& d);

} // namespace git_editor
