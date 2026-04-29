# Changelog

Check if your version update crosses a version with a wipe (e.g. Beta 1 -> Beta 4 crosses Beta 3 so it's wiped).
Crossing these versions will result in a database wipe:

- 1.0.0-beta.6
- 1.0.0-beta.3

## Known issues

- Failed exports may leave a `.tmp` zip next to the destination, compressed export also builds a `.sqlite-tmp` that is removed after packing. Incomplete runs are less likely to delete the existing `.gdge` before the new file is safely in place.

## 1.0.0-beta.7

- SQLite prepared statements finalized on all paths where it mattered, new commits that advance HEAD persist insert + HEAD in one transaction.
- Added import/export decompress cap for stored blob size
- Added atomic replace for zip exports on Windows
- Moved History and Levels load lists (and History rename) on to the git worker with a loading label. You can read more technical changes on GitHub.

## 1.0.0-beta.6

THIS UPDATE WIPES YOUR DATABASE!

- Added blob compression (with zlib). This also make the old `.gdge` file invalid.
- Added file size estimations in the mod's level page.
- Internal improvements like using more Geode's utils. You can read more technical changes on GitHub.

## 1.0.0-beta.5

- Check the GitHub commit for this one, it's mostly code changes/improvements.

## 1.0.0-beta.4

- Added default-on compression for exported `.gdge` files, stored as zips with automatic detection on import, both compressed and raw files are supported.
- Added more Node IDs (including popups: History, Levels, commit message, delta detail).
- Unified path system (`geode::utils::string::pathToString` via `PathUtf8`).
- Simplified revert action message for missing objects (Refactored revert payload system).
- Simplified in-memory caching system (LRU to map and flush-on-full).
- Vendored SQLite 3.53.0 amalgamation (was 3.38.2 via CPM).
- Removed legacy code and other internal improvements like `postToGitWorker` no longer wraps jobs in C++ `try`/`catch`. You can read more technical changes on GitHub.

## 1.0.0-beta.3

THIS UPDATE WIPES YOUR DATABASE!

- Switched level keying to Editor Level ID API (`cvolton.level-id-api`).
- Switched to Geode's async runtime for background git and database work.
- Removed legacy local/localid alias-key from runtime/store schema paths.
- Bumped commit DB schema version.

## 1.0.0-beta.2

- Replaced `.gdge` metadata integer parsing with non-throwing parsing.
- Replaced import/export and merge filename path conversions with UTF-8-safe handling.
- Added GitHub link in mod.json.

## 1.0.0-beta.1

- Initial version, expect edge cases. Report bugs at my Discord server.
