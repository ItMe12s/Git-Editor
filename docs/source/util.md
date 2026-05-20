# util

Helpers for background work, files, compression, and small text or hash tools.

## Main files

- `GitWorker.cpp`: background thread for heavy work
- `io/BlobCodec.cpp`: compress/decompress delta blobs (`optional` on failure, size cap before unpack)
- `io/DbZip.cpp`: zip wrap for `.gdge`, detect database vs zip, safe file replace on Windows
- `io/FileAtomic.cpp`: safe file replace helper
- `io/PathUtf8.hpp`: safe path strings for logs and UI
- `io/TextFileUtf8.cpp`: write UTF-8 text files (settings report export)
- `format/Parsing.cpp`: split and parse level text chunks
- `format/StateHash.cpp`: short hash of a level for tests and import checks
- `format/Shorten.hpp`: shorten long strings for UI

## Touches

Worker is used across service and UI actions. IO helpers are used by `CommitStore` and `GdgePackage`.

## You might care if

Contributors only, unless you debug slow History loads or export temp files.

## Code

- [src/util/GitWorker.cpp](../../src/util/GitWorker.cpp)
- [src/util/io/BlobCodec.cpp](../../src/util/io/BlobCodec.cpp)
- [src/util/io/DbZip.cpp](../../src/util/io/DbZip.cpp)
- [src/util/io/FileAtomic.cpp](../../src/util/io/FileAtomic.cpp)
- [src/util/format/Parsing.cpp](../../src/util/format/Parsing.cpp)
- [src/util/format/StateHash.cpp](../../src/util/format/StateHash.cpp)
