# Git Editor

A Geometry Dash (Geode SDK) mod: per-level commit history, checkout/revert/squash, level browser, and `.gdge` import/export/merge. Normal user info in [about.md](about.md).

## Other info not in about.md

- Linear history per level key, no branches.
- Commits store a JSON `Delta` against the parent in `git-editor.db` (under the mod save directory).
- Delta keys: `h` (header), `+` (adds), `-` (removes), `~` (modifies).
- State reconstruction replays from root to HEAD, with an in-memory state cache (default capacity 64, if the map is at capacity, inserting a new commit id clears the entire cache, see [StateCache.hpp](src/service/StateCache.hpp)).
- `GitService` returns [`Result<T>`](src/core/Result.hpp) (success payload in `value`, failure message in `error`). Shared DTOs (`ImportPlan`, `RevertPayload`, etc.) live in [ImportPlan.hpp](src/core/ImportPlan.hpp).
- Checkout is forward-only: inserts a new commit with `diff(HEAD, target)`.
- Revert applies `diff(target, target.parent)` onto current HEAD, reports conflicts.
- Squash requires 2+ contiguous selected commits.
- Editor levels use `id:<n>` as the level key, from **cvolton.level-id-api** (see [mod.json](mod.json) dependencies and [`levelKeyFor`](src/editor/LevelKey.cpp) in the source).
- Heavy git/DB operations run on a worker thread (`postToGitWorker`, see [GitWorker.cpp](src/util/GitWorker.cpp)) to keep the UI responsive. History list stats and package replay run in the service layer ([CommitSummaryBuilder](src/service/CommitSummaryBuilder.cpp), [PackageReconstruction](src/service/PackageReconstruction.cpp)).
- New commits that update HEAD use a single SQLite transaction (`insert` + `refs` update) so a failed HEAD update does not leave a stray commit row.
- [BlobCodec](src/util/io/BlobCodec.cpp) caps declared uncompressed size before allocating a decompression buffer (mitigates corrupt or hostile stored blobs). [DbZip](src/util/io/DbZip.cpp) replaces existing export paths with an atomic move on Windows (`MoveFileEx`) instead of delete-then-rename.
- Path strings for logs, SQLite, and on-screen file names use `geode::utils::string::pathToString`, mostly through [`PathUtf8.hpp`](src/util/io/PathUtf8.hpp) (`DbZip` uses `pathToString` directly in one error string).
- [DbZip.cpp](src/util/io/DbZip.cpp) peeks the first 16 bytes to classify a file as SQLite, zip, or unknown (used for `.gdge` on disk, not the live DB path). The running mod stores history only in a plain `git-editor.db` file.

### Source layout (high level)

| Folder | Role |
| ------ | ---- |
| `src/core/` | `Result`, import/revert DTOs |
| `src/model/`, `src/diff/`, `src/identity/` | Level state, deltas, matching |
| `src/store/` | SQLite commit DB, `.gdge` I/O |
| `src/service/` | `GitService`, merge, reconstruction, summary stats |
| `src/editor/` | Editor bridge, `levelKeyFor` |
| `src/ui/` | Popups; `ui/presentation/` for strings and notifications |
| `src/util/io/`, `src/util/format/` | Blob/zip/path I/O, parsing and hashing |
| `src/hooks/`, `src/settings/` | Geode entry points |
| `src/test/` | In-mod automated suites |

## TO-DO

- Password/encryption.
- Compression for entire database file as a single artifact (Distinct from per-commit blob zlib in beta.6).
- Git Stash.
- Specific header metadata support: Song and nong ID (Not needed for a solo project or collab but useful).

## Weird edge cases that might happen

- Main-thread reads vs worker writes on `sharedCommitStore` can still contend (e.g. `SQLITE_BUSY`), History/Levels mitigate jank by offloading the heaviest queries.
- `PRAGMA foreign_keys=ON;` is assumed, swapping in an unusual SQLite build could surprise you.

## Build

- **SQLite 3.53.0** is vendored at `src/sqlite/sqlite3.c`, built as a static library and linked into the mod. See [CMakeLists.txt](CMakeLists.txt).
- **ZLIB** is required for blob decompression/compression (CMake `find_package(ZLIB)` first, including CONFIG mode for vcpkg-style installs, FetchContent to zlib 1.3.2 if no suitable target is found).
- For manual testing, see [testing-checklist.md](testing-checklist.md).
