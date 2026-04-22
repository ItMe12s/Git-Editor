#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <string>

namespace git_editor {

// Read-only popup: scrollable multiline text (e.g. commit delta description).
class DeltaInfoLayer : public geode::Popup {
public:
    static DeltaInfoLayer* create(std::string title, std::string body);

protected:
    bool init(std::string title, std::string body);

    geode::ScrollLayer* m_scroll = nullptr;
};

} // namespace git_editor
