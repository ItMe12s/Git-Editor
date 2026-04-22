#include "DeltaInfoLayer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/cocos.hpp>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float kWidth       = 320.f;
constexpr float kHeight      = 200.f;
constexpr float kTitleBelow  = 48.f;
constexpr float kHMargin     = 14.f;
constexpr float kVMargin     = 8.f;
constexpr float kPanelPad    = 6.f;
constexpr float kTextScale   = 0.45f;
constexpr int   kPanelAlpha  = 128;

} // namespace

DeltaInfoLayer* DeltaInfoLayer::create(std::string title, std::string body) {
    auto* ret = new DeltaInfoLayer();
    if (ret && ret->init(std::move(title), std::move(body))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool DeltaInfoLayer::init(std::string title, std::string body) {
    if (!Popup::init(kWidth, kHeight)) return false;
    this->setTitle(title.c_str());

    float const panelW = kWidth  - kHMargin * 2.f;
    float const panelH = kHeight - kTitleBelow - kVMargin * 2.f;
    float const scrollW = panelW  - kPanelPad * 2.f;
    float const scrollH = panelH  - kPanelPad * 2.f;

    auto* panel = CCLayerColor::create({0, 0, 0, static_cast<GLubyte>(kPanelAlpha)}, panelW, panelH);
    panel->setAnchorPoint({0.f, 0.f});

    m_scroll = ScrollLayer::create({scrollW, scrollH});
    m_scroll->setAnchorPoint({0.f, 0.f});

    float const lineW   = (scrollW - 4.f) / kTextScale;
    auto* label         = CCLabelBMFont::create(
        body.c_str(), "chatFont.fnt", lineW, cocos2d::kCCTextAlignmentCenter
    );
    label->setScale(kTextScale);
    float const lh = label->getContentSize().height * kTextScale;
    float const ch = std::max(scrollH, lh + 8.f);
    m_scroll->m_contentLayer->setContentSize({scrollW, ch});
    label->setAnchorPoint({0.5f, 1.f});
    label->setPosition({scrollW * 0.5f, ch - 4.f});
    m_scroll->m_contentLayer->addChild(label);
    m_scroll->scrollToTop();

    panel->addChild(m_scroll);
    m_scroll->setPosition({kPanelPad, kPanelPad});

    m_mainLayer->addChildAtPosition(
        panel, Anchor::Center, {-panelW * 0.5f, -panelH * 0.55f}, false
    );
    return true;
}

} // namespace git_editor
