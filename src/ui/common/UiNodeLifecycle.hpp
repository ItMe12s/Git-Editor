#pragma once

#include <cocos2d.h>

namespace git_editor::ui_node_lifecycle {

inline bool isNodeActive(cocos2d::CCNode* node) {
    return node && node->getParent() && node->isRunning();
}

} // namespace git_editor::ui_node_lifecycle
