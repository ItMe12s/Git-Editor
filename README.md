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
- Heavy git/DB work is scheduled with `geode::async::runtime().spawnBlocking` (`postToGitWorker`), the UI can still read the store on the main thread. See comments on `CommitStore` and [GitWorker.cpp](src/util/GitWorker.cpp).

## TO-DO

- File compression + Password/encryption.
- Git Stash.
- ~~Less boilerplate on Git Service, Gd Header, Level Parser.~~
  - Combined parsers.
  - Added result template
- ~~Make more use or just remove some of State Cache, Conflict Kind.~~
  - Combined missing types into one.
- Finally clean up GdgeImportPlanner, LevelKeyResolver, HistorySelectionModel.
- ~~Should rename async queue to like job queue or something.~~
  - Renamed to GitWorker.

## Weird edge cases that might happen

- Main-thread and worker race on in-memory state because committing while reading history at the same time but this can't happen normally anyway.
- PRAGMA foreign_keys=ON; not an user issue, just in case someone replace the SQLite build with a one that doesn't support it.

## Build

- **SQLite** is pulled via **CPM** in [CMakeLists.txt](CMakeLists.txt) and linked into the mod.
- For manual testing, see [testing-checklist.md](testing-checklist.md).
