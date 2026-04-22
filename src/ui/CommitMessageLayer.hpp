#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/utils/function.hpp>
#include <string>

namespace geode {
    class TextInput;
}

namespace git_editor {

class CommitMessageLayer : public geode::Popup {
public:
    using ConfirmFn = geode::Function<void(std::string const&)>;
    using CloseFn   = geode::Function<void()>;

    static constexpr std::size_t kMaxMessageLen = 120;

    static CommitMessageLayer* create(
        ConfirmFn onConfirm,
        std::string title = "New Commit",
        std::string buttonLabel = "Commit",
        std::string initialText = "",
        CloseFn onClose = {}
    );

protected:
    bool init(
        ConfirmFn onConfirm,
        std::string title,
        std::string buttonLabel,
        std::string initialText,
        CloseFn onClose
    );

    void onConfirmClicked(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject* sender) override;

    geode::TextInput* m_input    = nullptr;
    ConfirmFn         m_callback;
    CloseFn           m_onClose;
    bool              m_submitted = false;
};

} // namespace git_editor
