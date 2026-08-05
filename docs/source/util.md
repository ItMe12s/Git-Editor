# util

## Summary

Helpers for background work, files, compression, and small text or hash tools.
Worker is used across service and UI actions. IO helpers are used by `CommitStore` and `GdgePackage`.

## Files

- `GitWorker.hpp` / `GitWorker.cpp`: `spawnOnGitWorker` and `postToGitWorker` via Geode `spawnBlocking` plus a mutex
- `io/BlobCodec.cpp`: compress/decompress delta blobs (`optional` on failure, size cap before unpack)
- `io/DbZip.cpp`: zip wrap for `.gdge`, detect database vs zip, safe file replace on Windows
- `io/FileAtomic.cpp`: safe file replace helper
- `format/Parsing.cpp`: split and parse level text chunks
- `format/StateHash.cpp`: short hash of a level for tests and import checks
- `format/Shorten.hpp`: shorten long strings for UI

## Notes

`GitWorker` is not a dedicated background thread.
It posts work through Geode async with a mutex so only one git job runs at a time.

## Related

- [README.md](README.md)
- [service.md](service.md)
- [store.md](store.md)
- [ui.md](ui.md)

## Source

- `src/util/GitWorker.cpp`
- `src/util/io/BlobCodec.cpp`
- `src/util/io/DbZip.cpp`
- `src/util/io/FileAtomic.cpp`
- `src/util/format/Parsing.cpp`
- `src/util/format/StateHash.cpp`
- `src/util/format/Shorten.hpp`
