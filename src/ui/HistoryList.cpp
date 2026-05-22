#include "HistoryLayer.hpp"

#include "HistoryCommitRow.hpp"
#include "common/ScrollListPopup.hpp"
#include "editor/LevelKey.hpp"
#include "service/GitService.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/cocos.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace git_editor {

namespace {

constexpr float histPopupWidth  = scroll_list_popup::Layout::kWidth;
constexpr float histPopupHeight = scroll_list_popup::Layout::kHeight;

struct HistoryLoadResult {
    LevelKey                   levelKey;
    std::vector<CommitSummary> commits;
};

HistoryLoadResult loadHistory(LevelKey levelKey, LevelKey const& activeEditorLevelKey) {
    auto commits = sharedGitService().listSummaries(levelKey);
    if (commits.empty() && !activeEditorLevelKey.empty() && activeEditorLevelKey != levelKey) {
        auto activeCommits = sharedGitService().listSummaries(activeEditorLevelKey);
        if (!activeCommits.empty()) {
            levelKey = activeEditorLevelKey;
            commits  = std::move(activeCommits);
        }
    }
    return { std::move(levelKey), std::move(commits) };
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
    if (!Popup::init(histPopupWidth, histPopupHeight)) return false;

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

void HistoryLayer::rebuildHeader() {
    if (m_listState.closing || !m_headerMenu) return;
    m_headerMenu->removeAllChildren();

    Ref<HistoryLayer> self(this);

    auto modeLabel = m_squashMode ? "Exit Squash" : "Squash Mode";
    auto modeTex   = m_squashMode ? "GJ_button_06.png" : "GJ_button_04.png";
    auto modeSpr   = ButtonSprite::create(modeLabel, "bigFont.fnt", modeTex, .8f);
    modeSpr->setScale(.45f);
    auto modeBtn = CCMenuItemExt::createSpriteExtra(modeSpr,
        [self](CCMenuItemSpriteExtra*) {
            if (!self) return;
            self->m_squashMode = !self->m_squashMode;
            self->m_selected.clear();
            self->rebuildHeader();
            if (self->m_commits.empty()) self->rebuildList();
            else                         self->renderList(self->m_commits);
        }
    );
    modeBtn->setID("git-editor-history-mode-btn"_spr);
    m_headerMenu->addChild(modeBtn);

    if (m_squashMode && m_selected.size() >= 2) {
        auto label = std::string("Squash ") + std::to_string(m_selected.size());
        auto spr   = ButtonSprite::create(label.c_str(), "bigFont.fnt", "GJ_button_01.png", .8f);
        spr->setScale(.45f);
        auto squashBtn = CCMenuItemExt::createSpriteExtra(spr,
            [self](CCMenuItemSpriteExtra*) {
                if (self) self->onSquashPressed();
            }
        );
        squashBtn->setID("git-editor-history-squash-btn"_spr);
        m_headerMenu->addChild(squashBtn);
    }

    m_headerMenu->updateLayout();
}

void HistoryLayer::rebuildList() {
    if (m_listState.closing || !m_scroll) return;

    auto* editor = m_editor.data();
    auto const activeKey = (editor && editor->m_level) ? levelKeyFor(editor->m_level) : "";
    Ref<HistoryLayer> self(this);
    std::string levelKey = m_levelKey;

    scroll_list_popup::loadAsync<HistoryLoadResult>(
        m_listState,
        m_scroll,
        "Loading commits...",
        "git-editor-history-loading"_spr,
        [levelKey, activeKey]() { return loadHistory(levelKey, activeKey); },
        [self](std::uint64_t serial) {
            return self && !scroll_list_popup::isStaleLoad(self->m_listState, serial);
        },
        [self](HistoryLoadResult loaded) mutable {
            self->m_levelKey = std::move(loaded.levelKey);
            self->renderList(std::move(loaded.commits));
        }
    );
}

void HistoryLayer::renderList(std::vector<CommitSummary> loadedCommits) {
    if (m_listState.closing || !m_scroll) return;

    auto* content = m_scroll->getContentLayer();
    content->removeAllChildren();
    m_commits = std::move(loadedCommits);
    auto const& commits = m_commits;

    float const rowWidth = content->getContentSize().width;

    if (commits.empty()) {
        scroll_list_popup::showCenteredLabel(
            content, "No commits yet.", "git-editor-history-empty"_spr
        );
        scroll_list_popup::resetScrollTop(m_scroll);
        return;
    }

    Ref<HistoryLayer> self(this);

    for (auto const& c : commits) {
        bool const selected = m_squashMode && m_selected.count(c.id) > 0;
        content->addChild(history_rows::createCommitRow(
            c, rowWidth, m_squashMode, selected, self
        ));
    }

    content->updateLayout();
    scroll_list_popup::resetScrollTop(m_scroll);
}

} // namespace git_editor
