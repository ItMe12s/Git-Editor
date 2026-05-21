#pragma once

#include <Geode/ui/Popup.hpp>
#include <alphalaneous.alphas-ui-pack/include/API.hpp>
#include <string>

namespace git_editor {

class DeltaInfoLayer : public geode::Popup {
public:
    static DeltaInfoLayer* create(std::string title, std::string body);

protected:
    bool init(std::string title, std::string body);

    alpha::ui::AdvancedScrollLayer* m_scroll = nullptr;
};

} // namespace git_editor
