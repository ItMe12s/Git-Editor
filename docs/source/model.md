# model

Turns a Geometry Dash level string into memory you can compare and merge.

## Main files

- `LevelState.hpp`: header fields and objects keyed by stable IDs
- `LevelParser.cpp`: splits level text into header and objects
- `GdHeader.hpp` / `GdHeader.cpp`: merges level settings and color channels for collab

## Touches

`GitService`, `MergeService`, and `diff/` read and write `LevelState`. The editor applies the result after checkout or import.

## You might care if

You see wrong header or channel data after a merge. Merge rules live here and in `GdHeader`.

## Code

- [src/model/LevelState.hpp](../../src/model/LevelState.hpp)
- [src/model/LevelParser.cpp](../../src/model/LevelParser.cpp)
- [src/model/GdHeader.cpp](../../src/model/GdHeader.cpp)
