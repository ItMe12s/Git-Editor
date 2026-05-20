# sqlite

Built-in copy of SQLite used by the mod. You do not install SQLite separately.

## Main files

- `sqlite3.c`: single combined source file (version **3.53.0**)
- `sqlite3.h`: public API

## Notes

[CMakeLists.txt](../../CMakeLists.txt) builds SQLite as a library linked into the mod, safe for multiple threads.

## Touches

`CommitStore` and `GdgePackage` open SQLite databases for `git-editor.db` and `.gdge`.

## You might care if

Contributors only.

## Code

- [src/sqlite/sqlite3.c](../../src/sqlite/sqlite3.c)
- [CMakeLists.txt](../../CMakeLists.txt)
