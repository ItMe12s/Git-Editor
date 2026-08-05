# ui

## Summary

Popups, confirm dialogs, and text shown in History and Levels.
Opened from hooks. Calls `GitService`. `LevelBrowserLayer` also calls `CommitStore` directly.

## Files

- `HistoryLayer.cpp`: history popup shell
- `HistoryList.cpp`: history popup chrome, commit list load/render
- `HistoryCommitRow.cpp`: per-commit row UI (actions, squash toggler)
- `HistoryFlows.cpp`: checkout, revert, squash flows
- `HistoryActions.cpp`: editor apply and conflict alert
- `CommitMessageLayer.cpp`: commit message, squash message, rename
- `LevelBrowserLayer.cpp`: level list, load, delete
- `DeltaInfoLayer.cpp`: what-changed viewer
- `common/GitUiActionRunner.hpp`: slow work in background, update screen when done
- `common/UiAction.hpp`: busy guard for UI actions
- `common/PreparedEditorFlow.hpp`: prepare on worker, apply to editor, finalize on worker
- `common/ScrollListPopup.hpp` / `ScrollListPopup.cpp`: shared scroll-list popup shell
- `common/UiNodeLifecycle.hpp`: node validity check
- `presentation/UiText.cpp`: labels and timestamps
- `presentation/DeltaText.cpp`: text describing what changed
- `presentation/DeltaColors.hpp`: colors for delta lines

## Notes

Commit messages are capped at `CommitMessageLayer::kMaxMessageLen = 120` in the UI.
The service layer does not enforce the length.

Player-facing button and popup labels live in [features/README.md](../features/README.md).

## Related

- [README.md](README.md)
- [hooks.md](hooks.md)
- [service.md](service.md)
- [../features/README.md](../features/README.md)
- [settings.md](settings.md)

## Source

- `src/ui/HistoryLayer.cpp`
- `src/ui/HistoryList.cpp`
- `src/ui/HistoryCommitRow.cpp`
- `src/ui/HistoryFlows.cpp`
- `src/ui/HistoryActions.cpp`
- `src/ui/CommitMessageLayer.cpp`
- `src/ui/LevelBrowserLayer.cpp`
- `src/ui/DeltaInfoLayer.cpp`
- `src/ui/common/GitUiActionRunner.hpp`
- `src/ui/common/UiAction.hpp`
- `src/ui/common/PreparedEditorFlow.hpp`
- `src/ui/common/ScrollListPopup.hpp`
- `src/ui/common/ScrollListPopup.cpp`
- `src/ui/common/UiNodeLifecycle.hpp`
- `src/ui/presentation/UiText.cpp`
- `src/ui/presentation/DeltaText.cpp`
- `src/ui/presentation/DeltaColors.hpp`
