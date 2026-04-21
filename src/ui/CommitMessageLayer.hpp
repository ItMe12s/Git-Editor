#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/utils/function.hpp>
#include <string>

namespace geode {
    class TextInput;
}

namespace git_editor {

// Popup: single-line message input + Commit / Cancel. Invokes `onConfirm`
// with the validated message when the user confirms.
class CommitMessageLayer : public geode::Popup {
public:
    using ConfirmFn = geode::Function<void(std::string const&)>;

    static constexpr std::size_t kMaxMessageLen = 120;

    static CommitMessageLayer* create(ConfirmFn onConfirm);

protected:
    bool init(ConfirmFn onConfirm);

    void onConfirmClicked(cocos2d::CCObject*);

    geode::TextInput* m_input    = nullptr;
    ConfirmFn         m_callback;
};

} // namespace git_editor
