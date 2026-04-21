// Git Editor - entry point.
//
// Runtime logic lives in:
//   - hooks/EditorPauseLayerHook.cpp (injects the Commit + History buttons)
//   - store/CommitStore.cpp          (SQLite persistence)
//   - editor/LevelStateIO.cpp        (capture / apply level string)
//   - ui/CommitMessageLayer.cpp      (commit message popup)
//   - ui/HistoryLayer.cpp            (history + checkout popup)
//
// The mod registers its hooks purely through the Geode $modify system, so no
// explicit `$on_mod(Loaded)` handler is required here.

#include <Geode/Geode.hpp>
