# Git Editor

A Geometry Dash (Geode) mod: per-level commit history, checkout/revert/squash, level browser, and `.gdge` import/export/merge. Normal user info in [about.md](about.md).

## Core model

- Linear history per level key, no branches.
- Commits store a JSON `Delta` against the parent in `git-editor.db` (under the mod save directory).
- Delta keys: `h` (header), `+` (adds), `-` (removes), `~` (modifies).
- State reconstruction replays from root to HEAD, with an in-memory state cache (default capacity 16, if the map is at capacity, inserting a new commit id clears the entire cache, see [StateCache.hpp](src/service/StateCache.hpp)).
- `GitService` returns [`Result<T>`](src/service/Result.hpp) (success payload in `value`, failure message in `error`). Payload structs in [GitService.hpp](src/service/GitService.hpp) group multi-field outcomes (e.g. revert with conflicts, multi-file `.gdge` import stats).

## Other info not in about.md

- Checkout is forward-only: inserts a new commit with `diff(HEAD, target)`.
- Revert applies `diff(target, target.parent)` onto current HEAD, reports conflicts.
- Squash requires 2+ contiguous selected commits.
- Editor levels use `id:<n>` as the level key, from **cvolton.level-id-api** (see [mod.json](mod.json) dependencies and `levelKeyFor` in the source).
- Heavy git/DB work is serialized on a worker via `postToGitWorker` (see [GitWorker.cpp](src/util/GitWorker.cpp)). The UI can still read the store on the main thread. See `CommitStore` and the worker header.
- Path strings for logs, SQLite, and on-screen file names use `geode::utils::string::pathToString`, mostly through [`PathUtf8.hpp`](src/util/PathUtf8.hpp) (`DbZip` uses `pathToString` directly in one error string). The mod save directory is created with `geode::utils::file::createDirectoryAll` before opening `git-editor.db`.
- The editor pause menu and History / Levels / commit message / delta popups set stable `imes.git-editor/…` node ids (mod-prefixed ids per Geode’s usual `"name"_spr` pattern) for compatibility, see [testing-checklist.md](testing-checklist.md).
- [DbZip.cpp](src/util/DbZip.cpp) peeks the first 16 bytes to classify a file as SQLite, zip, or unknown (used for `.gdge` on disk, not the live DB path). The running mod stores history only in a plain `git-editor.db` file.

## TO-DO

- ~~File compression.~~ Beta 4
- Password/encryption. Beta 7+
- Git Stash. Beta 6
- Specific header metadata support: Song and nong ID (Not needed for a solo project or collab but useful). Beta 6
- ~~Less boilerplate on Git Service, Gd Header, Level Parser.~~ Beta 4
  - Combined parsers.
  - Added result template.
  - Reworked Git Service, Revert return value.
- ~~Make more use or just remove some of State Cache, Conflict Kind.~~ Beta 4
  - Combined missing types into one.
  - Reworked LRU cache to just a simple map and flush-on-full.
- ~~Finally clean up LevelKeyResolver, HistorySelectionModel.~~ Beta 4
  - Removed LevelKeyResolver, canonical key still via resolveCanonicalKey.
  - Put history selection model in HistoryLayer.cpp.
  - GdgeImportPlanner keep separate from GitService (planning vs service).
- ~~Should rename async queue to like job queue or something.~~ Beta 4
  - Renamed to GitWorker.

## Weird edge cases that might happen

- Main thread and worker race on in-memory state if the user somehow commits while the UI is in a bugged overlapping state and would NEVER happen in game ever.
- `PRAGMA foreign_keys=ON;` is assumed, swapping in an unusual SQLite build could surprise you.

## Build

- **SQLite 3.53.0** is vendored at `src/sqlite/sqlite3.c` and compiled directly into the mod. See [CMakeLists.txt](CMakeLists.txt).
- For manual testing, see [testing-checklist.md](testing-checklist.md).
