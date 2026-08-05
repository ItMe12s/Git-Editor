# identity

## Summary

Gives each object a stable ID so the change tracker can tell edits apart from delete-plus-add.
Geometry Dash does not export real object IDs. This folder matches objects between saves.
Runs on commit when `GitService` captures the level.

## Files

- `Matcher.cpp`: assigns stable IDs (`assignUuids`, `assignFreshUuids`)

## Notes

Matching uses a fingerprint first (type, rounded x/y, rotation, groups).
If that fails, a spatial fallback runs within `kSpatialThreshold = 32.0`.

A huge reshape can still show many adds and removes instead of edits. That is normal, not a bug.

## Related

- [README.md](README.md)
- [model.md](model.md)
- [diff.md](diff.md)
- [service.md](service.md)
- [../known-issues.md](../known-issues.md)

## Source

- `src/identity/Matcher.hpp`
- `src/identity/Matcher.cpp`
