# service

## Summary

Main brain for history: commit, checkout, revert, squash, import, and merge.
Pause menu and popups go through `GitService`, except a few direct `CommitStore` calls in Levels.

## Files

- `GitService.cpp`: public API used by UI and hooks. Also builds text for the **? changes** viewer. Owns an inline reconstruct cache
- `MergeService.cpp`: `mergeStates3Way` using your version, a friend's version, and a shared starting point
- `GdgeImportPlanner.cpp`: `classifyImports` into auto-merge vs step-by-step import
- `GdgeImportMerge.cpp`: `prepareImportManyFromGdge` during a multi-file import
- `PackageReconstruction.cpp`: `reconstructPackageHead` from a `.gdge` package
- `CommitSummaryBuilder.cpp`: `buildCommitSummaries` for stats in the History list
- `PendingOps.hpp`: data for prepare-then-save flows (`Prepared`, pending head/squash/import)

## Notes

Heavy actions run in two steps. First, do the math off the main thread. Second, update the editor and save.
Checkout, revert, squash, load level, and multi-file import use this. Commit and export save in one step.

The reconstruct cache lives inside `GitService` (`m_cache`, default capacity 64).
It is not LRU. When full, the cache clears, then the new entry is inserted.

`.gdge` package build lives in [store.md](store.md) (`GdgeExport.cpp`).

## Related

- [README.md](README.md)
- [store.md](store.md)
- [diff.md](diff.md)
- [model.md](model.md)
- [ui.md](ui.md)
- [hooks.md](hooks.md)
- [../features/README.md](../features/README.md)

## Source

- `src/service/GitService.hpp`
- `src/service/GitService.cpp`
- `src/service/MergeService.cpp`
- `src/service/GdgeImportPlanner.cpp`
- `src/service/GdgeImportMerge.cpp`
- `src/service/PackageReconstruction.cpp`
- `src/service/CommitSummaryBuilder.cpp`
- `src/service/PendingOps.hpp`
