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

} // namespace git_editor
