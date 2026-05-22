#include "HistoryLayer.hpp"

#include "common/ScrollListPopup.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float kHistPopupWidth  = scroll_list_popup::Layout::kWidth;
constexpr float kHistPopupHeight = scroll_list_popup::Layout::kHeight;

} // namespace

HistoryLayer* HistoryLayer::create(
    std::string levelKey,
    LevelEditorLayer* editor,
    EditorPauseLayer* pauseLayer
) {
    auto ret = new HistoryLayer();
    if (ret && ret->init(std::move(levelKey), editor, pauseLayer)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool HistoryLayer::init(
    std::string levelKey,
    LevelEditorLayer* editor,
    EditorPauseLayer* pauseLayer
) {
    if (!Popup::init(kHistPopupWidth, kHistPopupHeight)) return false;

    m_levelKey   = std::move(levelKey);
    m_editor     = editor;
    m_pauseLayer = pauseLayer;

    this->setTitle("History");

    m_scroll = scroll_list_popup::attachScrollList(
        this, m_mainLayer, "git-editor-history-scroll"_spr
    );

    m_headerMenu = CCMenu::create();
    m_headerMenu->setID("git-editor-history-header-menu"_spr);
    m_headerMenu->setContentSize({200.f, 26.f});
    m_headerMenu->setAnchorPoint({1.f, .5f});
    m_headerMenu->setLayout(
        RowLayout::create()
            ->setGap(6.f)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisOverflow(true)
    );
    m_mainLayer->addChildAtPosition(m_headerMenu, Anchor::TopRight, {-12.f, -16.f});

    this->rebuildHeader();
    this->rebuildList();
    return true;
}

void HistoryLayer::onClose(CCObject* sender) {
    scroll_list_popup::markClosing(m_listState, m_scroll);
    m_headerMenu = nullptr;
    Popup::onClose(sender);
}

bool HistoryLayer::closeOnce(CCObject* sender) {
    return scroll_list_popup::closeOnce(this, m_listState, sender, [this](CCObject* s) { onClose(s); });
}

} // namespace git_editor
