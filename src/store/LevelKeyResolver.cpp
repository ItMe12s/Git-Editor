#include "LevelKeyResolver.hpp"

namespace git_editor::level_key_resolver {

LevelKey resolveCanonicalKey(LevelKey const& observedKey) {
    return observedKey;
}

} // namespace git_editor::level_key_resolver
