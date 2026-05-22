#pragma once

#include <Geode/ui/Notification.hpp>

namespace git_editor {

inline bool tryBeginBusyAction(bool& busyFlag) {
    if (busyFlag) {
        geode::Notification::create("Action already running", geode::NotificationIcon::Info)->show();
        return false;
    }
    busyFlag = true;
    return true;
}

inline void finishBusyAction(bool& busyFlag) {
    busyFlag = false;
}

inline bool exitBusyIfClosing(bool& busy, bool closing) {
    if (!closing) return false;
    finishBusyAction(busy);
    return true;
}

} // namespace git_editor
