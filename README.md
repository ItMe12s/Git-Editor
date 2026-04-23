# Git Editor

A Geometry Dash (Geode) mod: per-level commit history, checkout/revert/squash, level browser, and `.gdge` import/export/merge. Normal user info in [about.md](about.md).

## Core model

- Linear history per level key, no branches.
- Commits store a JSON `Delta` against the parent in `git-editor.db` (under the mod save directory).
- Delta keys: `h` (header), `+` (adds), `-` (removes), `~` (modifies).
- State reconstruction replays from root to HEAD, with an in-memory state cache (capacity 16 by default, LRU-ordered, see [StateCache.hpp](src/service/StateCache.hpp)).
- `GitService` returns [`Result<T>`](src/service/Result.hpp) (success payload in `value`, failure message in `error`). Payload structs in [GitService.hpp](src/service/GitService.hpp) group multi-field outcomes (e.g. revert with conflicts, multi-file `.gdge` import stats).

## Other info not in about.md

- Checkout is forward-only: inserts a new commit with `diff(HEAD, target)`.
- Revert applies `diff(target, target.parent)` onto current HEAD, reports conflicts.
- Squash requires 2+ contiguous selected commits.
- Editor levels use `id:<n>` as the level key, from **cvolton.level-id-api** (see [mod.json](mod.json) dependencies and `levelKeyFor` in the source).
- Heavy git/DB work is serialized on a worker (`postToGitWorker` in [GitWorker.cpp](src/util/GitWorker.cpp)); the UI can still read the store on the main thread. See comments on `CommitStore` and the worker header.

## TO-DO

- File compression + Password/encryption. Beta 5
- Git Stash. Beta 6+
- Specific header metadata support: Song and nong ID (Not needed for a solo project or collab but useful). Beta 6+
- ~~Less boilerplate on Git Service, Gd Header, Level Parser.~~ Beta 4
  - Combined parsers.
  - Added result template.
  - Reworked Git Service, Revert return value.
- ~~Make more use or just remove some of State Cache, Conflict Kind.~~ Beta 4
  - Combined missing types into one.
  - State cache rework still not done ong.
- Finally clean up GdgeImportPlanner, LevelKeyResolver, HistorySelectionModel. Beta 4
- ~~Should rename async queue to like job queue or something.~~ Beta 4
  - Renamed to GitWorker.

## Weird edge cases that might happen

- Main thread and worker race on in-memory state if the user somehow commits while the UI is in a bugged overlapping state and would NEVER happen in game ever.
- `PRAGMA foreign_keys=ON;` is assumed, swapping in an unusual SQLite build could surprise you.

## Build

- **SQLite** is pulled via **CPM** in [CMakeLists.txt](CMakeLists.txt) and linked into the mod.
- For manual testing, see [testing-checklist.md](testing-checklist.md).
