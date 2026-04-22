#include "LevelBrowserLayer.hpp"

#include "../store/CommitStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/cocos.hpp>

#include <ctime>
#include <string>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float kPopupWidth     = 420.f;
constexpr float kPopupHeight    = 280.f;
constexpr float kListPadX       = 20.f;
constexpr float kListPadTop     = 36.f;
constexpr float kListPadBottom  = 16.f;
constexpr float kRowHeight      = 50.f;

std::string formatTimestamp(std::int64_t unixSeconds) {
    std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm tm{};
#if defined(_WIN32)
    if (localtime_s(&tm, &t) != 0) return "?";
#else
    if (localtime_r(&t, &tm) == nullptr) return "?";
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm) == 0) return "?";
    return std::string(buf);
}

std::string shorten(std::string const& s, std::size_t maxChars) {
    if (s.size() <= maxChars) return s;
    return s.substr(0, maxChars - 1) + "...";
}

} // namespace

LevelBrowserLayer* LevelBrowserLayer::create() {
    auto ret = new LevelBrowserLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LevelBrowserLayer::init() {
    if (!Popup::init(kPopupWidth, kPopupHeight)) return false;

    this->setTitle("Levels");

    float const innerW = kPopupWidth - kListPadX * 2.f;
    float const innerH = kPopupHeight - kListPadTop - kListPadBottom;

    m_scroll = ScrollLayer::create({innerW, innerH});
    m_scroll->setAnchorPoint({0.f, 0.f});
    m_scroll->m_contentLayer->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setGap(3.f)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisOverflow(false)
            ->setAutoGrowAxis(std::optional<float>(innerH))
    );

    m_mainLayer->addChildAtPosition(
        m_scroll, Anchor::Center,
        {-innerW / 2.f, -innerH / 2.f + 2.f},
        false
    );

    this->rebuildList();
    return true;
}

void LevelBrowserLayer::rebuildList() {
    if (!m_scroll) return;

    auto* content = m_scroll->m_contentLayer;
    content->removeAllChildren();

    auto levels = sharedCommitStore().listLevels();
    float const rowWidth = content->getContentSize().width;

    if (levels.empty()) {
        auto empty = CCLabelBMFont::create("No levels with commits.", "bigFont.fnt");
        empty->setScale(.5f);
        empty->setOpacity(160);
        content->addChild(empty);
        content->updateLayout();
        m_scroll->scrollToTop();
        return;
    }

    Ref<LevelBrowserLayer> self(this);

    auto makeDeleteBtn = [](geode::Function<void(CCMenuItemSpriteExtra*)> cb)
        -> CCMenuItemSpriteExtra* {
        auto spr = ButtonSprite::create("Delete", "bigFont.fnt", "GJ_button_06.png", .8f);
        spr->setScale(.4f);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    };

    for (auto const& lv : levels) {
        auto row = CCNode::create();
        row->setContentSize({rowWidth, kRowHeight});
        row->setAnchorPoint({0.f, 0.f});
        row->setLayout(AnchorLayout::create());

        auto bg = CCLayerColor::create({0, 0, 0, 60}, rowWidth, kRowHeight);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({.5f, .5f});
        row->addChildAtPosition(bg, Anchor::Center);

        auto keyLbl = CCLabelBMFont::create(shorten(lv.levelKey, 40).c_str(), "chatFont.fnt");
        keyLbl->setScale(.55f);
        keyLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(keyLbl, Anchor::Left, {6.f, 10.f});

        std::string sub = std::to_string(lv.commitCount) + " commits - "
            + formatTimestamp(lv.lastCreatedAt);
        auto subLbl = CCLabelBMFont::create(sub.c_str(), "chatFont.fnt");
        subLbl->setScale(.45f);
        subLbl->setOpacity(200);
        subLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(subLbl, Anchor::Left, {6.f, -10.f});

        auto menu = CCMenu::create();
        menu->setContentSize({80.f, kRowHeight});
        menu->setAnchorPoint({1.f, .5f});
        menu->setLayout(
            RowLayout::create()
                ->setGap(4.f)
                ->setAxisAlignment(AxisAlignment::End)
                ->setCrossAxisOverflow(true)
        );

        auto const levelKey = lv.levelKey;
        auto const count    = lv.commitCount;

        auto delBtn = makeDeleteBtn([self, levelKey, count](CCMenuItemSpriteExtra*) {
            createQuickPopup(
                "Delete level history",
                ("Remove all " + std::to_string(count) + " commits for \"" + shorten(levelKey, 48)
                 + "\"? Irreversible.")
                    .c_str(),
                "Cancel", "Delete",
                [self, levelKey](FLAlertLayer*, bool yes) {
                    if (!yes) return;
                    if (!sharedCommitStore().deleteLevel(levelKey)) {
                        Notification::create("Delete failed", NotificationIcon::Error)->show();
                        return;
                    }
                    Notification::create("Level history removed", NotificationIcon::Success)
                        ->show();
                    if (self) self->rebuildList();
                }
            );
        });
        menu->addChild(delBtn);
        menu->updateLayout();
        row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});

        content->addChild(row);
    }

    content->updateLayout();
    m_scroll->scrollToTop();
}

} // namespace git_editor
