#pragma once

#include <Geode/binding/GJGameLevel.hpp>
#include <string>

namespace git_editor {

// id:<n> when m_levelID is non-zero, else name:<fnv1a hex> (rename forks history).
std::string levelKeyFor(GJGameLevel* level);

} // namespace git_editor
