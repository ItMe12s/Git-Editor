# ui

Popups, confirm dialogs, and text shown in History and Levels.

## Main files

- `CommitMessageLayer.cpp`: commit message, squash message, rename
- `HistoryList.cpp`: history popup chrome, commit list load/render
- `HistoryFlows.cpp`: checkout, revert, squash flows
- `HistoryActions.cpp`: editor apply and conflict alert
- `LevelBrowserLayer.cpp`: level list, load, delete
- `DeltaInfoLayer.cpp`: what-changed viewer
- `common/GitUiActionRunner.hpp`: slow work in background, update screen when done
- `common/UiAction.hpp`: busy guard for UI actions
- `presentation/UiText.cpp`: labels and timestamps
- `presentation/DeltaText.cpp`: text describing what changed

## Touches

Opened from [hooks.md](hooks.md). Calls `GitService`, `LevelBrowserLayer` also calls `CommitStore` directly.

## You might care if

You use History, Levels, or Commit in-game. See [features/README.md](../features/README.md).

## Code

- [src/ui/HistoryLayer.hpp](../../src/ui/HistoryLayer.hpp)
- [src/ui/HistoryList.cpp](../../src/ui/HistoryList.cpp)
- [src/ui/HistoryFlows.cpp](../../src/ui/HistoryFlows.cpp)
- [src/ui/LevelBrowserLayer.cpp](../../src/ui/LevelBrowserLayer.cpp)
- [src/ui/CommitMessageLayer.cpp](../../src/ui/CommitMessageLayer.cpp)
- [src/ui/common/GitUiActionRunner.hpp](../../src/ui/common/GitUiActionRunner.hpp)
- [src/ui/common/UiAction.hpp](../../src/ui/common/UiAction.hpp)
