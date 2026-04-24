# Changelog

Check if your version update crosses a version with a wipe (e.g. Beta 1 -> Beta 4 crosses Beta 3 so it's wiped).
Crossing these versions will result in a database wipe:

- 1.0.0-beta.3

## 1.0.0-beta.4

- Added default-on compression for exported `.gdge` files, stored as zips with automatic detection on import, both compressed and raw files are supported.
- Simplified revert action message for missing objects (Refactored revert payload system).
- Simplified in-memory caching system (LRU to map and flush-on-full).
- Vendored SQLite 3.53.0 amalgamation (was 3.38.2 via CPM).
- Removed legacy code and other internal improvements. You can read more technical changes on GitHub.

## 1.0.0-beta.3

THIS UPDATE WIPES YOUR DATABASE

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
