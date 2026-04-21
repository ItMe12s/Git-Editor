#pragma once

#include <Geode/binding/GJGameLevel.hpp>
#include <string>

namespace git_editor {

// Derive a stable, storage-safe key for the given level.
// Saved levels use their numeric id ("id:<n>").
// Unsaved / custom levels fall back to an FNV-1a hash of the level name
// ("name:<16-hex>") so renames still break history deliberately.
std::string levelKeyFor(GJGameLevel* level);

} // namespace git_editor
