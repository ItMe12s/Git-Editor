# sqlite

## Summary

Built-in copy of SQLite used by the mod. You do not install SQLite separately.
`CommitStore` and `GdgePackage` open databases for `git-editor.db` and `.gdge`.

## Files

- `sqlite3.c`: single combined source file (version **3.53.0**)
- `sqlite3.h`: public API

## Notes

`CMakeLists.txt` builds SQLite as a static library linked into the mod. Compile defines include:

- `SQLITE_THREADSAFE=1`
- `SQLITE_DEFAULT_MEMSTATUS=0`
- `SQLITE_MAX_LENGTH=1000000000`
- `SQLITE_DQS=0`
- `SQLITE_OMIT_LOAD_EXTENSION`
- `SQLITE_OMIT_DEPRECATED`

## Related

- [README.md](README.md)
- [store.md](store.md)
- [../building.md](../building.md)

## Source

- `src/sqlite/sqlite3.c`
- `CMakeLists.txt`
