#include "HistoryCommitRow.hpp"

#include "CommitMessageLayer.hpp"
#include "DeltaInfoLayer.hpp"
#include "HistoryLayer.hpp"
#include "common/GitUiActionRunner.hpp"
#include "common/UiNodeLifecycle.hpp"
#include "presentation/DeltaColors.hpp"
#include "presentation/UiText.hpp"
#include "service/GitService.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>

using namespace geode::prelude;

namespace git_editor::history_rows {

namespace {

CCMenuItemSpriteExtra* makeBtn(
    char const* label,
    char const* texture,
    geode::Function<void(CCMenuItemSpriteExtra*)> cb
) {
    auto spr = ButtonSprite::create(label, "bigFont.fnt", texture, .8f);
    spr->setScale(.4f);
    return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
}

} // namespace

CCNode* createCommitRow(
    CommitSummary const& c,
    float rowWidth,
    bool squashMode,
    bool selected,
    Ref<HistoryLayer> self
) {
    auto row = CCNode::create();
    row->setID("git-editor-history-row"_spr);
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

    auto statsNode = CCNode::create();
    statsNode->setContentSize({110.f, 12.f});
    statsNode->setAnchorPoint({0.f, .5f});
    statsNode->setLayout(
        RowLayout::create()
            ->setGap(4.f)
            ->setAxisAlignment(AxisAlignment::Start)
            ->setCrossAxisOverflow(false)
    );
    {
        auto makeStat = [](std::string const& text, ccColor3B color) {
            auto* lbl = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
            lbl->setScale(.45f);
            lbl->setColor(color);
            return lbl;
        };
        if (c.headerCount > 0) {
            statsNode->addChild(makeStat("h" + std::to_string(c.headerCount), kHdrColor));
        }
        if (c.addCount > 0) {
            statsNode->addChild(makeStat("+" + std::to_string(c.addCount), kAddColor));
        }
        if (c.modifyCount > 0) {
            statsNode->addChild(makeStat("~" + std::to_string(c.modifyCount), kModColor));
        }
        if (c.removeCount > 0) {
            statsNode->addChild(makeStat("-" + std::to_string(c.removeCount), kDelColor));
        }
    }
    statsNode->updateLayout();
    float const timeWidth = timeLbl->getContentSize().width * timeLbl->getScale();
    row->addChildAtPosition(statsNode, Anchor::Left, {10.f + timeWidth, 11.f});

    auto msgLbl = CCLabelBMFont::create(
        shorten(c.message, 34).c_str(), "chatFont.fnt"
    );
    msgLbl->setScale(.55f);
    msgLbl->setAnchorPoint({0.f, .5f});
    row->addChildAtPosition(msgLbl, Anchor::Left, {6.f, -8.f});

    auto menu = CCMenu::create();
    menu->setID("git-editor-history-row-menu"_spr);
    menu->setContentSize({210.f, kRowHeight});
    menu->setAnchorPoint({1.f, .5f});
    menu->setLayout(
        RowLayout::create()
            ->setGap(4.f)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisOverflow(true)
    );

    auto const commitId  = c.id;
    auto const commitMsg = c.message;

    if (squashMode) {
        auto tickBtn = CCMenuItemExt::createTogglerWithStandardSprites(
            .6f,
            [self, commitId](CCMenuItemToggler*) {
                if (!self) return;
                // Track m_selected, not isToggled(). GD binding mismatch.
                if (self->m_selected.count(commitId)) self->m_selected.erase(commitId);
                else                                  self->m_selected.insert(commitId);
                self->rebuildHeader();
            }
        );
        tickBtn->toggle(selected);
        menu->addChild(tickBtn);
    } else {
        auto helpBtn = makeBtn(
            "?", "GJ_button_04.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (!self || self->m_listState.closing) return;

                std::string title = "What changed";
                if (!commitMsg.empty()) {
                    title += " - ";
                    title += shorten(commitMsg, 24);
                }

                DeltaInfoLayer::createAndLoad(std::move(title), [commitId]() {
                    return sharedGitService().describeCommitChanges(commitId);
                });
            }
        );
        menu->addChild(helpBtn);

        auto renameBtn = makeBtn(
            "Rename", "GJ_button_04.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (auto popup = CommitMessageLayer::create(
                    [self, commitId](std::string const& newMessage) {
                        ui_action_runner::runWorkerResult<bool>(
                            [commitId, newMessage]() {
                                return sharedGitService().updateCommitMessage(commitId, newMessage);
                            },
                            [self](bool ok) {
                                if (!self || self->m_listState.closing) return;
                                if (!ok) {
                                    Notification::create("Rename failed", NotificationIcon::Error)->show();
                                    return;
                                }
                                Notification::create("Renamed commit", NotificationIcon::Success)->show();
                                if (ui_node_lifecycle::isNodeActive(self.data())) {
                                    self->rebuildList();
                                }
                            }
                        );
                    },
                    "Rename Commit",
                    "Save",
                    commitMsg
                )) {
                    popup->show();
                }
            }
        );
        menu->addChild(renameBtn);

        auto checkoutBtn = makeBtn(
            "Checkout", "GJ_button_02.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (self) self->startCheckoutFlow(commitId, commitMsg);
            }
        );
        menu->addChild(checkoutBtn);

        auto revertBtn = makeBtn(
            "Revert", "GJ_button_06.png",
            [self, commitId, commitMsg](CCMenuItemSpriteExtra*) {
                if (self) self->startRevertFlow(commitId, commitMsg);
            }
        );
        menu->addChild(revertBtn);
    }

    menu->updateLayout();
    row->addChildAtPosition(menu, Anchor::Right, {-6.f, 0.f});
    return row;
}

} // namespace git_editor::history_rows
