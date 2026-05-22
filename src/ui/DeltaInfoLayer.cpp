#include "DeltaInfoLayer.hpp"

#include "common/GitUiActionRunner.hpp"
#include "common/UiNodeLifecycle.hpp"
#include "presentation/DeltaColors.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float kWidth       = 400.f;
constexpr float kScrollViewH = 200.f;
constexpr float kTitleBelow  = 32.f;
constexpr float kHMargin     = 14.f;
constexpr float kTopPad      = 2.f;
constexpr float kBottomPad   = 6.f;
constexpr float kPanelPad    = 4.f;
constexpr float kTextScale   = 0.45f;
constexpr float kLineStep    = 11.f;
constexpr float kNavH        = 24.f;
constexpr int   kPanelAlpha  = 128;
constexpr std::size_t kLinesPerBlock = 250;

void splitBodyLines(std::string const& body, std::vector<std::string>& lines) {
    lines.clear();
    if (body.empty()) {
        lines.emplace_back();
        return;
    }
    std::size_t i = 0;
    while (i <= body.size()) {
        auto const end = body.find('\n', i);
        auto const lineEnd = (end == std::string::npos) ? body.size() : end;
        if (lineEnd > i) {
            lines.emplace_back(body.substr(i, lineEnd - i));
        } else if (lineEnd == i && i < body.size()) {
            lines.emplace_back();
        }
        if (end == std::string::npos) break;
        i = end + 1;
    }
}

std::size_t blockCountFor(std::size_t lineCount) {
    if (lineCount == 0) return 1;
    return (lineCount + kLinesPerBlock - 1) / kLinesPerBlock;
}

} // namespace

DeltaInfoLayer* DeltaInfoLayer::createAndLoad(
    std::string title,
    geode::Function<Result<std::string>(void)> loadFn
) {
    auto* popup = new DeltaInfoLayer();
    if (!popup || !popup->init(std::move(title), "")) {
        delete popup;
        return nullptr;
    }
    popup->autorelease();

    popup->show();

    Ref<DeltaInfoLayer> self(popup);
    auto loadFnHolder = std::make_shared<geode::Function<Result<std::string>(void)>>(std::move(loadFn));
    ui_action_runner::runWorkerResult<Result<std::string>>(
        [loadFnHolder]() { return (*loadFnHolder)(); },
        [self](Result<std::string> res) mutable {
            if (!self || !ui_node_lifecycle::isNodeActive(self.data())) return;
            if (!res.ok) {
                self->showLoadError(res.error);
                return;
            }
            self->applyBody(std::move(res.value));
        }
    );

    return popup;
}

bool DeltaInfoLayer::init(std::string title, std::string body) {
    float const panelW = kWidth - kHMargin * 2.f;
    float const panelH = kPanelPad * 2.f + kNavH + kScrollViewH;
    float const popupH = kTitleBelow + kTopPad + panelH + kBottomPad;

    if (!Popup::init(kWidth, popupH)) return false;
    this->setTitle(title.c_str());

    m_scrollW = panelW - kPanelPad * 2.f;
    m_scrollH = kScrollViewH;

    auto* wrapper = CCNode::create();
    wrapper->setID("git-editor-delta-wrapper"_spr);
    wrapper->setContentSize({panelW, panelH});
    wrapper->setAnchorPoint({0.5f, 0.f});

    auto* scrollBg = CCLayerColor::create(
        {0, 0, 0, static_cast<GLubyte>(kPanelAlpha)}, m_scrollW, m_scrollH
    );
    scrollBg->setID("git-editor-delta-scroll-bg"_spr);
    scrollBg->setAnchorPoint({0.f, 0.f});
    scrollBg->setPosition({kPanelPad, kPanelPad + kNavH});
    wrapper->addChild(scrollBg);

    m_scroll = alpha::ui::AdvancedScrollLayer::create({m_scrollW, m_scrollH});
    m_scroll->setID("git-editor-delta-scroll"_spr);
    m_scroll->setAnchorPoint({0.f, 0.f});
    m_scroll->setPosition({0.f, 0.f});
    scrollBg->addChild(m_scroll);

    Ref<DeltaInfoLayer> self(this);

    auto makeNavBtn = [](char const* label, char const* tex,
                         geode::Function<void(CCMenuItemSpriteExtra*)> cb) {
        auto spr = ButtonSprite::create(label, "bigFont.fnt", tex, .8f);
        spr->setScale(.38f);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    };

    m_prevBtn = makeNavBtn("<", "GJ_button_04.png", [self](CCMenuItemSpriteExtra*) {
        if (self) self->onPrevBlock();
    });
    m_nextBtn = makeNavBtn(">", "GJ_button_04.png", [self](CCMenuItemSpriteExtra*) {
        if (self) self->onNextBlock();
    });

    m_btnMenu = CCMenu::create();
    m_btnMenu->setID("git-editor-delta-nav-btns"_spr);
    m_btnMenu->addChild(m_prevBtn);
    m_btnMenu->addChild(m_nextBtn);
    m_btnMenu->alignItemsHorizontallyWithPadding(6.f);
    wrapper->addChildAtPosition(
        m_btnMenu, Anchor::BottomLeft,
        {kPanelPad + 22.f, kPanelPad + kNavH * 0.5f}
    );

    m_pageLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_pageLabel->setScale(.42f);
    m_pageLabel->setID("git-editor-delta-page"_spr);
    m_pageLabel->setAnchorPoint({1.f, 0.5f});
    wrapper->addChildAtPosition(
        m_pageLabel, Anchor::BottomRight,
        {-kPanelPad, kPanelPad + kNavH * 0.5f}
    );

    m_mainLayer->addChildAtPosition(wrapper, Anchor::Bottom, {0.f, kBottomPad}, false);

    if (body.empty()) showLoading();
    else {
        splitBodyLines(body, m_lines);
        showBlock(0);
    }
    return true;
}

void DeltaInfoLayer::onClose(CCObject* sender) {
    Popup::onClose(sender);
}

void DeltaInfoLayer::showLoading() {
    if (!m_scroll) return;

    clearOverlay();
    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();
    m_scroll->setInnerContentSize({m_scrollW, m_scrollH});

    auto* loading = CCLabelBMFont::create("Loading changes...", "bigFont.fnt");
    loading->setID("git-editor-delta-overlay"_spr);
    loading->setScale(.5f);
    loading->setOpacity(160);
    m_scroll->addChildAtPosition(loading, Anchor::Center);
    m_scroll->setScrollY(0);

    if (m_pageLabel) m_pageLabel->setString("Loading...");
    if (m_prevBtn) m_prevBtn->setEnabled(false);
    if (m_nextBtn) m_nextBtn->setEnabled(false);
}

void DeltaInfoLayer::clearOverlay() {
    if (m_scroll) m_scroll->removeChildByID("git-editor-delta-overlay"_spr);
}

void DeltaInfoLayer::applyBody(std::string body) {
    splitBodyLines(body, m_lines);
    showBlock(0);
}

void DeltaInfoLayer::showLoadError(std::string error) {
    if (!m_scroll) return;

    clearOverlay();
    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();
    m_scroll->setInnerContentSize({m_scrollW, m_scrollH});

    auto* lbl = CCLabelBMFont::create(error.c_str(), "chatFont.fnt");
    lbl->setID("git-editor-delta-overlay"_spr);
    lbl->setScale(.45f);
    m_scroll->addChildAtPosition(lbl, Anchor::Center);
    m_scroll->setScrollY(0);

    if (m_pageLabel) m_pageLabel->setString("Error");
    if (m_prevBtn) m_prevBtn->setEnabled(false);
    if (m_nextBtn) m_nextBtn->setEnabled(false);
}

void DeltaInfoLayer::showBlock(std::size_t blockIndex) {
    if (!m_scroll) return;

    clearOverlay();

    auto const blocks = blockCountFor(m_lines.size());
    m_blockIndex      = std::min(blockIndex, blocks - 1);

    auto const lineStart = m_blockIndex * kLinesPerBlock;
    auto const lineEnd   = std::min(lineStart + kLinesPerBlock, m_lines.size());

    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();

    std::size_t lineCount = 0;
    for (std::size_t li = lineStart; li < lineEnd; ++li) {
        if (!m_lines[li].empty()) ++lineCount;
    }

    float const contentH = std::max(m_scrollH, lineCount * kLineStep + 2.f);
    float y = contentH - kLineStep;

    for (std::size_t li = lineStart; li < lineEnd; ++li) {
        auto const& line = m_lines[li];
        if (line.empty()) continue;

        auto* lineLbl = CCLabelBMFont::create(line.c_str(), "chatFont.fnt");
        lineLbl->setScale(kTextScale);
        lineLbl->setColor(colorForDeltaLine(line));
        lineLbl->setAnchorPoint({0.f, 0.f});
        lineLbl->setPosition({4.f, y});
        content->addChild(lineLbl);
        y -= kLineStep;
    }

    m_scroll->setInnerContentSize({m_scrollW, contentH});
    m_scroll->setScrollY(0);

    if (m_pageLabel) {
        std::size_t const displayStart = m_lines.empty() ? 0 : lineStart + 1;
        std::size_t const displayEnd   = lineEnd;
        char buf[96];
        if (m_lines.empty()) {
            std::snprintf(buf, sizeof(buf), "Block 1 / 1");
        } else {
            std::snprintf(
                buf, sizeof(buf), "Block %zu / %zu  (lines %zu-%zu)",
                m_blockIndex + 1, blocks, displayStart, displayEnd
            );
        }
        m_pageLabel->setString(buf);
    }

    if (m_prevBtn) m_prevBtn->setEnabled(m_blockIndex > 0);
    if (m_nextBtn) m_nextBtn->setEnabled(m_blockIndex + 1 < blocks);
}

void DeltaInfoLayer::onPrevBlock() {
    if (m_blockIndex == 0) return;
    showBlock(m_blockIndex - 1);
}

void DeltaInfoLayer::onNextBlock() {
    showBlock(m_blockIndex + 1);
}

} // namespace git_editor
