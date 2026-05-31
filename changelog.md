# Changelog

[**Check the project out on GitHub!**](https://github.com/ItMe12s/Git-Editor)

Consider giving it a star and check the commit history for a more technical changelog.

## Known issues

- Failed exports may leave a `.tmp` zip next to the destination, compressed export also builds a `.sqlite-tmp` that is removed after packing. Incomplete runs are less likely to delete the existing `.gdge` before the new file is safely in place.
- Some updates will not be able to properly read old `.gdge` files.

---

## 1.0.0-beta.12

- Bigger action/commit size (SQLite max length is now 1,000,000,000), basically uncapped.
- Improved error feedback.

## 1.0.0-beta.11 - The "Better for All" Update

This update focuses on the project's codebase, making it much easier for anyone to contribute.
Most of the codebase has been reworked, and full documentation is now available.

If you're already using this then you can expect better long-term support, stability, and future improvements.

- UI/UX improvements, no more massive stuttering when clicking buttons while working on a big level.
- New "What changed" menu, diff texts no longer cap out or lag, and you can now flip through them like pages.
- Fixed crashes, performance issues, and the bug where the game gets stuck on a black screen when exiting the editor.
- Lots of internal cleanup for stability and speed.

## 1.0.0-beta.10

Going forward, I'll be adding auto-migrations once the mod leaves beta.
However, migration scripts likely won't be needed as the save format is now finalized.
TL;DR You can daily drive this mod.

- New icon and about page.

## 1.0.0-beta.9

- Fixed crash when opening "Edit Object" on a startpos (actually any objects that uses kA*/kS*/whatever) after a revert or checkout.
- Added round-trip regression test for start-pos `kA*` keys.

## 1.0.0-beta.8

- Added automated test, check settings menu.
- Increased state cache size.
- Internal improvements like reusing prepared statement and level parser optimization.

## 1.0.0-beta.7

- SQLite prepared statements finalized on all paths where it mattered, new commits that advance HEAD persist insert + HEAD in one transaction.
- Added import/export decompress cap for stored blob size.
- Added atomic replace for zip exports on Windows.
- Moved History and Levels load lists (and History rename) on to the git worker with a loading label.

## 1.0.0-beta.6

THIS UPDATE WIPES YOUR DATABASE!

- Added blob compression (with zlib). This also make the old `.gdge` file invalid.
- Added file size estimations in the mod's level page.
- Internal improvements like using more Geode's utils.

## 1.0.0-beta.5

- Check the GitHub commit for this one, it's mostly code changes/improvements.

## 1.0.0-beta.4

- Added default-on compression for exported `.gdge` files, stored as zips with automatic detection on import, both compressed and raw files are supported.
- Added more Node IDs (including popups: History, Levels, commit message, delta detail).
- Unified path system (`geode::utils::string::pathToString` via `PathUtf8`).
- Simplified revert action message for missing objects (Refactored revert payload system).
- Simplified in-memory caching system (LRU to map and flush-on-full).
- Vendored SQLite 3.53.0 amalgamation (was 3.38.2 via CPM).
- Removed legacy code and other internal improvements like `postToGitWorker` no longer wraps jobs in C++ `try`/`catch`.

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
