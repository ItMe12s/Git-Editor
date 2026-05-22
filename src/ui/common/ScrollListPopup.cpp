#include "ScrollListPopup.hpp"

#include "UiNodeLifecycle.hpp"

#include <Geode/ui/Layout.hpp>

using namespace geode::prelude;

namespace git_editor::scroll_list_popup {

float innerWidth() {
    return Layout::kWidth - Layout::kPadX * 2.f;
}

float innerHeight() {
    return Layout::kHeight - Layout::kPadTop - Layout::kPadBottom;
}

alpha::ui::AdvancedScrollLayer* attachScrollList(
    geode::Popup* popup,
    cocos2d::CCNode* mainLayer,
    char const* scrollId
) {
    if (!popup || !mainLayer) return nullptr;

    float const w = innerWidth();
    float const h = innerHeight();

    auto* scroll = alpha::ui::AdvancedScrollLayer::create({w, h});
    scroll->setID(scrollId);
    scroll->setAnchorPoint({0.f, 0.f});
    scroll->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setGap(3.f)
            ->setCrossAxisOverflow(false)
            ->setAutoGrowAxis(std::optional<float>(h))
    );

    mainLayer->addChildAtPosition(
        scroll, Anchor::Center,
        {-w * .5f, -h * .55f},
        false
    );
    return scroll;
}

void markClosing(ListState& state, alpha::ui::AdvancedScrollLayer*& scroll) {
    if (state.closing) return;
    state.closing = true;
    ++state.loadSerial;
    scroll = nullptr;
}

bool closeOnce(
    geode::Popup* popup,
    ListState const& state,
    cocos2d::CCObject* sender,
    std::function<void(cocos2d::CCObject*)> onClose
) {
    if (state.closing || !ui_node_lifecycle::isNodeActive(popup)) return false;
    onClose(sender);
    return true;
}

bool isStaleLoad(ListState const& state, std::uint64_t serial) {
    return state.closing || serial != state.loadSerial;
}

void showCenteredLabel(
    cocos2d::CCNode* content,
    char const* text,
    char const* labelId,
    float opacity
) {
    if (!content) return;
    auto* label = CCLabelBMFont::create(text, "bigFont.fnt");
    label->setID(labelId);
    label->setScale(.5f);
    label->setOpacity(static_cast<GLubyte>(opacity));
    content->addChild(label);
    content->updateLayout();
}

void resetScrollTop(alpha::ui::AdvancedScrollLayer* scroll) {
    if (scroll) scroll->setScrollY(0);
}

std::uint64_t beginLoading(
    ListState& state,
    alpha::ui::AdvancedScrollLayer* scroll,
    char const* loadingText,
    char const* loadingId
) {
    auto* content = scroll->getContentLayer();
    content->removeAllChildren();
    showCenteredLabel(content, loadingText, loadingId);
    return ++state.loadSerial;
}

} // namespace git_editor::scroll_list_popup
