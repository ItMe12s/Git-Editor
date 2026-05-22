# store

Saves commit history on disk. One live database and portable level packages.

## Main files

- `CommitStore.cpp`: `git-editor.db` in the mod save folder
- `CommitSchema.cpp`: database layout version
- `GdgePackage.cpp`: `.gdge` export and import (SQLite, optional zip)

## Notes

- `git-editor.db`: all levels on your PC (normal SQLite database file)
- `.gdge`: one level's history to share or back up (can be zipped)
- Reads hand back ready-to-use change data. Writes compress before they save. Summary lists hand back the raw compressed blob so the History popup can load fast and only unpack what it shows.
- `.gdge` read and write both return a `Result` so callers can show a clear error if a file is broken.

## Touches

`GitService` is the main caller. Levels popup calls `CommitStore` for level list, delete, and db path.

## You might care if

You back up `git-editor.db` or trade `.gdge` files with collaborators.

## Code

- [src/store/CommitStore.cpp](../../src/store/CommitStore.cpp)
- [src/store/GdgePackage.cpp](../../src/store/GdgePackage.cpp)
