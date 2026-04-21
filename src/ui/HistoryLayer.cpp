#include "HistoryLayer.hpp"

#include "../editor/LevelStateIO.hpp"
#include "../store/CommitStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/cocos.hpp>

#include <cerrno>
#include <cstdio>
#include <ctime>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float kPopupWidth    = 400.f;
constexpr float kPopupHeight   = 260.f;
constexpr float kListPadX      = 20.f;
constexpr float kListPadTop    = 36.f;
constexpr float kListPadBottom = 16.f;
constexpr float kRowHeight     = 38.f;

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
    if (!Popup::init(kPopupWidth, kPopupHeight)) return false;

    m_levelKey   = std::move(levelKey);
    m_editor     = editor;
    m_pauseLayer = pauseLayer;

    this->setTitle("History");

    float const innerW = kPopupWidth  - kListPadX * 2.f;
    float const innerH = kPopupHeight - kListPadTop - kListPadBottom;

    m_scroll = ScrollLayer::create({innerW, innerH});
    m_scroll->setAnchorPoint({0.f, 0.f});
    m_scroll->m_contentLayer->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)   // newest on top
            ->setGap(3.f)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisOverflow(false)
            ->setAutoGrowAxis(std::optional<float>(innerH))
    );

    m_mainLayer->addChildAtPosition(
        m_scroll, Anchor::Center,
        { -innerW / 2.f, -innerH / 2.f + 2.f },
        /* useAnchorLayout */ false
    );

    this->rebuildList();
    return true;
}

void HistoryLayer::rebuildList() {
    if (!m_scroll) return;

    auto* content = m_scroll->m_contentLayer;
    content->removeAllChildren();

    auto commits = sharedCommitStore().listCommits(m_levelKey);

    float const rowWidth = content->getContentSize().width;

    if (commits.empty()) {
        auto empty = CCLabelBMFont::create("No commits yet.", "bigFont.fnt");
        empty->setScale(.5f);
        empty->setOpacity(160);
        content->addChild(empty);
    } else {
        // Stable, non-owning snapshots of the collaborators we'll need inside
        // the Checkout confirmation lambda. Capturing `this` would risk
        // dangling access if HistoryLayer gets torn down before the user
        // answers the confirm dialog.
        auto* editor     = m_editor;
        auto* pauseLayer = m_pauseLayer;
        Ref<HistoryLayer> self(this);

        for (auto const& c : commits) {
            auto row = CCNode::create();
            row->setContentSize({rowWidth, kRowHeight});
            row->setAnchorPoint({0.f, 0.f});
            row->setLayout(AnchorLayout::create());

            auto bg = CCLayerColor::create({0, 0, 0, 60}, rowWidth, kRowHeight);
            bg->ignoreAnchorPointForPosition(false);
            bg->setAnchorPoint({.5f, .5f});
            row->addChildAtPosition(bg, Anchor::Center);

            auto timeLbl = CCLabelBMFont::create(
                formatTimestamp(c.createdAt).c_str(), "chatFont.fnt"
            );
            timeLbl->setScale(.5f);
            timeLbl->setAnchorPoint({0.f, .5f});
            row->addChildAtPosition(timeLbl, Anchor::Left, {6.f, 8.f});

            auto msgLbl = CCLabelBMFont::create(
                shorten(c.message, 32).c_str(), "chatFont.fnt"
            );
            msgLbl->setScale(.55f);
            msgLbl->setAnchorPoint({0.f, .5f});
            row->addChildAtPosition(msgLbl, Anchor::Left, {6.f, -8.f});

            auto menu = CCMenu::create();
            menu->setContentSize({80.f, kRowHeight});
            menu->setAnchorPoint({1.f, .5f});

            auto spr = ButtonSprite::create("Checkout", "bigFont.fnt", "GJ_button_02.png", .8f);
            spr->setScale(.45f);

            auto const commitId = c.id;
            auto btn = CCMenuItemExt::createSpriteExtra(
                spr,
                [self, editor, pauseLayer, commitId](CCMenuItemSpriteExtra*) {
                    createQuickPopup(
                        "Checkout",
                        "This will <cr>replace</c> all current objects in the editor. "
                        "Continue?",
                        "Cancel", "Checkout",
                        [self, editor, pauseLayer, commitId](FLAlertLayer*, bool yes) {
                            if (!yes) return;

                            auto commit = sharedCommitStore().getCommit(commitId);
                            if (!commit) {
                                Notification::create(
                                    "Commit not found", NotificationIcon::Error
                                )->show();
                                return;
                            }
                            if (!applyLevelString(editor, commit->levelString)) {
                                Notification::create(
                                    "Checkout failed", NotificationIcon::Error
                                )->show();
                                return;
                            }
                            Notification::create(
                                "Checked out", NotificationIcon::Success
                            )->show();

                            // Close this popup, then the pause layer so the
                            // user lands back in the editor with the
                            // restored objects. `self` is a Ref so the
                            // popup is guaranteed alive for this call.
                            if (self) self->onClose(nullptr);
                            if (pauseLayer) pauseLayer->onResume(nullptr);
                        }
                    );
                }
            );
            btn->setAnchorPoint({1.f, .5f});
            menu->addChild(btn);
            row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});

            content->addChild(row);
        }
    }

    content->updateLayout();
    m_scroll->scrollToTop();
}

} // namespace git_editor
