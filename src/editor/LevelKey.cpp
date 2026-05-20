#include "LevelKey.hpp"

#include <cvolton.level-id-api/include/EditorIDs.hpp>

namespace git_editor {

std::string levelKeyFor(GJGameLevel* level) {
    if (!level) {
        return "invalid:no-level";
    }

    auto const editorId = EditorIDs::getID(level);
    return "id:" + std::to_string(editorId);
}

} // namespace git_editor
