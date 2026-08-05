# model

## Summary

Turns a Geometry Dash level string into memory you can compare and merge.
`GitService`, `MergeService`, and `diff/` read and write `LevelState`.
The editor applies the result after checkout or import.

## Files

- `LevelState.hpp`: header fields and objects keyed by stable IDs
- `LevelParser.cpp`: splits level text into header and objects
- `GdHeader.hpp` / `GdHeader.cpp`: merges level settings and color channels for collab (`mergeHeaders3Way`)

## Notes

Wrong header or channel data after a merge usually points here or at `GdHeader`.

## Related

- [README.md](README.md)
- [diff.md](diff.md)
- [identity.md](identity.md)
- [service.md](service.md)
- [editor.md](editor.md)

## Source

- `src/model/LevelState.hpp`
- `src/model/LevelParser.cpp`
- `src/model/GdHeader.cpp`
