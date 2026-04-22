#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

namespace git_editor {

class LevelBrowserLayer : public geode::Popup {
public:
    static LevelBrowserLayer* create();

protected:
    bool init();

    void rebuildList();

    geode::ScrollLayer* m_scroll = nullptr;
};

} // namespace git_editor
