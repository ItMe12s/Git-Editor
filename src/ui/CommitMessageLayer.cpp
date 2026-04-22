#include "CommitMessageLayer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CCTextInputNode.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/cocos.hpp>

#include <cctype>

using namespace geode::prelude;

namespace git_editor {

namespace {

std::string trim(std::string s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isSpace(s.front())) s.erase(s.begin());
    while (!s.empty() && isSpace(s.back()))  s.pop_back();
    return s;
}

} // namespace

CommitMessageLayer* CommitMessageLayer::create(
    ConfirmFn onConfirm,
    std::string title,
    std::string buttonLabel,
    std::string initialText
) {
    auto ret = new CommitMessageLayer();
    if (ret && ret->init(
        std::move(onConfirm), std::move(title), std::move(buttonLabel), std::move(initialText)
    )) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CommitMessageLayer::init(
    ConfirmFn onConfirm,
    std::string title,
    std::string buttonLabel,
    std::string initialText
) {
    constexpr float kWidth  = 340.f;
    constexpr float kHeight = 160.f;

    if (!Popup::init(kWidth, kHeight)) return false;

    m_callback = std::move(onConfirm);

    this->setTitle(title.c_str());

    m_input = TextInput::create(kWidth - 60.f, "Commit message", "chatFont.fnt");
    m_input->setMaxCharCount(static_cast<size_t>(kMaxMessageLen));
    m_input->setCommonFilter(CommonFilter::Any);
    if (!initialText.empty()) {
        m_input->setString(initialText);
    }
    m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {0.f, 6.f});

    auto confirmSpr = ButtonSprite::create(buttonLabel.c_str(), "bigFont.fnt", "GJ_button_01.png", .8f);
    confirmSpr->setScale(.7f);
    auto confirmBtn = CCMenuItemSpriteExtra::create(
        confirmSpr, this,
        menu_selector(CommitMessageLayer::onConfirmClicked)
    );
    m_buttonMenu->addChildAtPosition(confirmBtn, Anchor::Bottom, {0.f, 28.f});

    return true;
}

void CommitMessageLayer::onConfirmClicked(CCObject*) {
    if (!m_input) return;

    auto raw = m_input->getString();
    auto message = trim(std::string(raw.c_str(), raw.size()));

    if (message.empty()) {
        Notification::create("Message required", NotificationIcon::Warning)->show();
        return;
    }
    if (message.size() > kMaxMessageLen) {
        message.resize(kMaxMessageLen);
    }

    if (m_callback) m_callback(message);

    this->onClose(nullptr);
}

} // namespace git_editor
