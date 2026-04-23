# Git Editor

A Geometry Dash (Geode) mod: per-level commit history, checkout/revert/squash, level browser, and `.gdge` import/export/merge. Normal user info in [about.md](about.md).

## Core model

- Linear history per level key, no branches.
- Commits store a JSON `Delta` against the parent in `git-editor.db` (under the mod save directory).
- Delta keys: `h` (header), `+` (adds), `-` (removes), `~` (modifies).
- State reconstruction replays root -> HEAD, with an LRU cache (default 16 states).

## Other info not in about.md

- Checkout is forward-only: inserts a new commit with `diff(HEAD, target)`.
- Revert applies `diff(target, target.parent)` onto current HEAD, reports conflicts.
- Squash requires 2+ contiguous selected commits.
- Editor levels use `id:<n>` as the level key, from **cvolton.level-id-api** (see [mod.json](mod.json) dependencies and `levelKeyFor` in the source).
- Heavy git/DB work is scheduled with `geode::async::runtime().spawnBlocking` (`postToGitWorker`), the UI can still read the store on the main thread. See comments on `CommitStore` and [AsyncQueue.cpp](src/util/AsyncQueue.cpp).

## TO-DO

- File compression + Password/encryption
- Git Stash
- Less boilerplate on Git Service, Gd Header, Level Parser
- Make more use of State Cache, Conflict Kind
- Finally clean up GdgeImportPlanner, LevelKeyResolver, HistorySelectionModel
- Should rename async queue to like job queue or something

## Build

- **SQLite** is pulled via **CPM** in [CMakeLists.txt](CMakeLists.txt) and linked into the mod.
- For manual testing, see [testing-checklist.md](testing-checklist.md).
