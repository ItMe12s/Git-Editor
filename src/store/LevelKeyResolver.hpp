#pragma once

#include "CommitStore.hpp"

namespace git_editor::level_key_resolver {

LevelKey resolveCanonicalKey(LevelKey const& observedKey);

} // namespace git_editor::level_key_resolver
