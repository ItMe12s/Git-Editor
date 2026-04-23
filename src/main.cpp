// Git Editor, hihi, check the /ui/ folder first.

#include "util/DbFlusher.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>

$execute {
    geode::GameEvent(geode::GameEventType::Exiting).listen([]() {
        git_editor::flushLocalDbNow();
        return true;
    }).leak();
}
