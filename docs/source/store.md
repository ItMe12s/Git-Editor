# store

## Summary

Saves commit history on disk. One live database and portable level packages.
`GitService` is the main caller. Levels popup also calls `CommitStore` for level list, delete, and db path.

## Files

- `CommitStore.cpp`: `git-editor.db` in the mod save folder
- `CommitSchema.cpp`: schema create and migrate via `commit_schema::ensureSchema`
- `GdgePackage.cpp`: `.gdge` read and write (SQLite, optional zip)
- `GdgeExport.cpp`: `buildGdgePackageFromCommits` (CommitStore rows to package data)

## Notes

- `git-editor.db`: all levels on your PC
- `.gdge`: one level's history to share or back up
- Schema version is `CommitStore::kSchemaVersion = 5`
- Reads hand back ready-to-use change data. Writes compress before they save.
  Summary lists hand back the raw compressed blob so History can load fast and only unpack what it shows
- `.gdge` read and write both return a `Result` so callers can show a clear error if a file is broken

## Related

- [README.md](README.md)
- [service.md](service.md)
- [sqlite.md](sqlite.md)
- [util.md](util.md)
- [../known-issues.md](../known-issues.md)

## Source

- `src/store/CommitStore.cpp`
- `src/store/CommitStore.hpp`
- `src/store/CommitSchema.cpp`
- `src/store/GdgePackage.cpp`
- `src/store/GdgeExport.cpp`
