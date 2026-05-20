# store

Saves commit history on disk. One live database and portable level packages.

## Main files

- `CommitStore.cpp`: `git-editor.db` in the mod save folder
- `CommitSchema.cpp`: database layout version
- `GdgePackage.cpp`: `.gdge` export and import (SQLite, optional zip)

## Notes

- `git-editor.db`: all levels on your PC (normal SQLite database file)
- `.gdge`: one level's history to share or back up (can be zipped)

## Touches

`GitService` is the main caller. History and Levels popups call `CommitStore` for list and delete actions.

## You might care if

You back up `git-editor.db` or trade `.gdge` files with collaborators.

## Code

- [src/store/CommitStore.cpp](../../src/store/CommitStore.cpp)
- [src/store/GdgePackage.cpp](../../src/store/GdgePackage.cpp)
