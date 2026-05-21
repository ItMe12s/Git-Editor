# service

Main brain for history: commit, checkout, revert, squash, import, export, and merge.

## Main files

- `GitService.cpp`: public API used by UI and hooks
- `MergeService.cpp`: merge using your version, a friend's version, and a shared starting point
- `GdgeImportPlanner.cpp`: groups import files into auto-merge vs step-by-step import
- `ReconstructionService.hpp`: rebuild level state by walking the commit chain
- `StateCache.cpp`: keeps up to 64 recently rebuilt levels in memory for speed
- `CommitSummaryBuilder.cpp`: stats shown in the History list
- `PackageReconstruction.cpp`: rebuild state from a `.gdge` package
- `PendingOps.hpp`: data for prepare-then-save flows (`Prepared`, pending head/squash/import)

## Notes

Two-phase prepare then finalize (compute first, save after the editor updates) applies to checkout, revert, squash, load level, and import-many. Commit and export save in one step.

## Touches

Pause menu and popups go through `GitService`, except a few direct `CommitStore` calls in Levels.

## You might care if

Contributors only, unless you debug import plans or merge conflicts.

## Code

- [src/service/GitService.hpp](../../src/service/GitService.hpp)
- [src/service/GitService.cpp](../../src/service/GitService.cpp)
- [src/service/MergeService.cpp](../../src/service/MergeService.cpp)
- [src/service/ReconstructionService.hpp](../../src/service/ReconstructionService.hpp)
