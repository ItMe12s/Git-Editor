#pragma once

#include <Geode/ui/Popup.hpp>
#include <alphalaneous.alphas-ui-pack/include/API.hpp>

#include <cocos2d.h>
#include <cstddef>
#include <string>
#include <vector>

namespace git_editor {

class DeltaInfoLayer : public geode::Popup {
public:
    static DeltaInfoLayer* create(std::string title, std::string body = "");

    void applyBody(std::string body);
    void showLoadError(std::string error);

protected:
    bool init(std::string title, std::string body);

    void showLoading();
    void clearOverlay();
    void showBlock(std::size_t blockIndex);
    void onPrevBlock();
    void onNextBlock();

    std::vector<std::string>        m_lines;
    std::size_t                     m_blockIndex = 0;
    float                           m_scrollW    = 0.f;
    float                           m_scrollH    = 0.f;
    alpha::ui::AdvancedScrollLayer* m_scroll     = nullptr;
    cocos2d::CCMenu*                m_btnMenu    = nullptr;
    cocos2d::CCLabelBMFont*         m_pageLabel  = nullptr;
    cocos2d::CCMenuItem*            m_prevBtn    = nullptr;
    cocos2d::CCMenuItem*            m_nextBtn    = nullptr;
};

} // namespace git_editor
