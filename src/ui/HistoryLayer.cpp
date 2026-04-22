#include "HistoryLayer.hpp"

#include "../editor/LevelStateIO.hpp"
#include "../service/GitService.hpp"
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

constexpr float kPopupWidth    = 420.f;
constexpr float kPopupHeight   = 280.f;
constexpr float kListPadX      = 20.f;
constexpr float kListPadTop    = 36.f;
constexpr float kListPadBottom = 16.f;
constexpr float kRowHeight     = 46.f;

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

CCNode* makeBadge(char const* text) {
    auto lbl = CCLabelBMFont::create(text, "chatFont.fnt");
    lbl->setScale(.4f);
    lbl->ignoreAnchorPointForPosition(false);
    lbl->setAnchorPoint({ .5f, .5f });
    return lbl;
}

void showConflictSummary(std::vector<Conflict> const& conflicts) {
    if (conflicts.empty()) return;

    int adds = 0, removes = 0, missingTargets = 0, stale = 0;
    for (auto const& c : conflicts) {
        switch (c.kind) {
            case Conflict::Kind::AddAlreadyExists: ++adds;           break;
            case Conflict::Kind::RemoveMissing:    ++removes;        break;
            case Conflict::Kind::ModifyMissing:    ++missingTargets; break;
            case Conflict::Kind::ModifyStale:      ++stale;          break;
        }
    }

    std::string body = "Some ops could not be applied cleanly:\n";
    if (adds)           body += "- " + std::to_string(adds)           + " add(s) already present\n";
    if (removes)        body += "- " + std::to_string(removes)        + " remove(s) already gone\n";
    if (missingTargets) body += "- " + std::to_string(missingTargets) + " modify(ies) targeting missing objects\n";
    if (stale)          body += "- " + std::to_string(stale)          + " stale field(s) skipped";

    FLAlertLayer::create(
        "Revert - partial",
        body.c_str(),
        "OK"
    )->show();
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
            ->setAxisReverse(true)
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

    auto commits = sharedCommitStore().list(m_levelKey);

    float const rowWidth = content->getContentSize().width;

    if (commits.empty()) {
        auto empty = CCLabelBMFont::create("No commits yet.", "bigFont.fnt");
        empty->setScale(.5f);
        empty->setOpacity(160);
        content->addChild(empty);
        content->updateLayout();
        m_scroll->scrollToTop();
        return;
    }

    auto* editor     = m_editor;
    auto* pauseLayer = m_pauseLayer;
    std::string levelKey = m_levelKey;
    Ref<HistoryLayer> self(this);

    auto makeBtn = [](char const* label, char const* texture,
                      geode::Function<void(CCMenuItemSpriteExtra*)> cb) -> CCMenuItemSpriteExtra* {
        auto spr = ButtonSprite::create(label, "bigFont.fnt", texture, .8f);
        spr->setScale(.4f);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    };

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
        row->addChildAtPosition(timeLbl, Anchor::Left, {6.f, 11.f});

        auto msgLbl = CCLabelBMFont::create(
            shorten(c.message, 34).c_str(), "chatFont.fnt"
        );
        msgLbl->setScale(.55f);
        msgLbl->setAnchorPoint({0.f, .5f});
        row->addChildAtPosition(msgLbl, Anchor::Left, {6.f, -8.f});

        if (c.reverts) {
            auto const* label = (c.message.rfind("Revert", 0) == 0) ? "revert" : "checkout";
            auto badge = makeBadge(label);
            row->addChildAtPosition(badge, Anchor::TopLeft, { 30.f, -8.f });
        }

        auto menu = CCMenu::create();
        menu->setContentSize({120.f, kRowHeight});
        menu->setAnchorPoint({1.f, .5f});
        menu->setLayout(
            RowLayout::create()
                ->setGap(4.f)
                ->setAxisAlignment(AxisAlignment::End)
                ->setCrossAxisOverflow(true)
        );

        auto const commitId = c.id;
        auto const commitMsg = c.message;

        auto checkoutBtn = makeBtn(
            "Checkout", "GJ_button_02.png",
            [self, editor, pauseLayer, levelKey, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                createQuickPopup(
                    "Checkout",
                    ("Load state of commit \"" + shorten(commitMsg, 40) +
                     "\"? A new auto-revert commit will be added on top of HEAD.").c_str(),
                    "Cancel", "Checkout",
                    [self, editor, pauseLayer, levelKey, commitId](FLAlertLayer*, bool yes) {
                        if (!yes) return;
                        auto outcome = sharedGitService().checkout(levelKey, commitId);
                        if (!outcome.ok) {
                            Notification::create(
                                ("Checkout failed: " + outcome.error).c_str(),
                                NotificationIcon::Error
                            )->show();
                            return;
                        }
                        if (!applyLevelState(editor, outcome.state)) {
                            Notification::create(
                                "Checkout applied to DB but editor refused",
                                NotificationIcon::Warning
                            )->show();
                        } else {
                            Notification::create("Checked out", NotificationIcon::Success)->show();
                        }
                        if (self) self->onClose(nullptr);
                        if (pauseLayer) pauseLayer->onResume(nullptr);
                    }
                );
            }
        );
        menu->addChild(checkoutBtn);

        auto revertBtn = makeBtn(
            "Revert", "GJ_button_06.png",
            [self, editor, pauseLayer, levelKey, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                createQuickPopup(
                    "Revert",
                    ("Undo just the changes from commit \"" + shorten(commitMsg, 40) +
                     "\"? Later commits are preserved.").c_str(),
                    "Cancel", "Revert",
                    [self, editor, pauseLayer, levelKey, commitId](FLAlertLayer*, bool yes) {
                        if (!yes) return;
                        auto outcome = sharedGitService().revert(levelKey, commitId);
                        if (!outcome.ok) {
                            Notification::create(
                                ("Revert failed: " + outcome.error).c_str(),
                                NotificationIcon::Error
                            )->show();
                            return;
                        }
                        if (!applyLevelState(editor, outcome.state)) {
                            Notification::create(
                                "Revert applied to DB but editor refused",
                                NotificationIcon::Warning
                            )->show();
                        } else if (outcome.conflicts.empty()) {
                            Notification::create("Reverted", NotificationIcon::Success)->show();
                        } else {
                            Notification::create(
                                "Reverted with conflicts", NotificationIcon::Warning
                            )->show();
                        }
                        if (self) self->onClose(nullptr);
                        if (pauseLayer) pauseLayer->onResume(nullptr);
                        showConflictSummary(outcome.conflicts);
                    }
                );
            }
        );
        menu->addChild(revertBtn);

        menu->updateLayout();
        row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});

        content->addChild(row);
    }

    content->updateLayout();
    m_scroll->scrollToTop();
}

} // namespace git_editor
