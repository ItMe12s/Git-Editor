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

    static constexpr std::size_t kMaxMessageLen = 120;

    static CommitMessageLayer* create(ConfirmFn onConfirm);

protected:
    bool init(ConfirmFn onConfirm);

    void onConfirmClicked(cocos2d::CCObject*);

    geode::TextInput* m_input    = nullptr;
    ConfirmFn         m_callback;
};

} // namespace git_editor
