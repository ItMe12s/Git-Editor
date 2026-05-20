#pragma once

#include <Geode/binding/GJGameLevel.hpp>
#include <string>

namespace git_editor {

// id:<n> for editor levels via cvolton.level-id-api.
// Null level returns invalid:no-level.
std::string levelKeyFor(GJGameLevel* level);

} // namespace git_editor
