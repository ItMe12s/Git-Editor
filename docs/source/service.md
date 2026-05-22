# service

Main brain for history: commit, checkout, revert, squash, import, export, and merge.

## Main files

- `GitService.cpp`: public API used by UI and hooks. Also builds the text for the **? changes** viewer
- `MergeService.cpp`: merge using your version, a friend's version, and a shared starting point
- `GdgeImportPlanner.cpp`: groups import files into auto-merge vs step-by-step import
- `GdgeImportMerge.cpp`: rebuilds state and runs the merge during a multi-file import
- `GdgeExport.cpp`: writes the current level's history to a `.gdge` package
- `ReconstructionService.hpp`: rebuild level state by walking the commit chain
- `StateCache.cpp`: keeps up to 64 recently rebuilt levels in memory for speed
- `CommitSummaryBuilder.cpp`: stats shown in the History list
- `PackageReconstruction.cpp`: rebuild state from a `.gdge` package
- `PendingOps.hpp`: data for prepare-then-save flows (`Prepared`, pending head/squash/import)

## Notes

Heavy actions run in two steps. First, do the math on a background thread. Second, update the editor and save. This keeps the game smooth on big levels. Checkout, revert, squash, load level, and multi-file import use this. Commit and export save in one step.

## Touches

Pause menu and popups go through `GitService`, except a few direct `CommitStore` calls in Levels.

## You might care if

Contributors only, unless you debug import plans or merge conflicts.

## Code

- [src/service/GitService.hpp](../../src/service/GitService.hpp)
- [src/service/GitService.cpp](../../src/service/GitService.cpp)
- [src/service/MergeService.cpp](../../src/service/MergeService.cpp)
- [src/service/ReconstructionService.hpp](../../src/service/ReconstructionService.hpp)
