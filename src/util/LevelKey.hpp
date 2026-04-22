#pragma once

#include <Geode/binding/GJGameLevel.hpp>
#include <string>

namespace git_editor {

// id:<n> when m_levelID is non-zero, else local:<name-hash>:<session-hash>.
// Null level returns invalid:no-level.
std::string levelKeyFor(GJGameLevel* level);

} // namespace git_editor
