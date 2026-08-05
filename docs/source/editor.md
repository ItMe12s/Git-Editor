# editor

## Summary

Bridge between the Geometry Dash editor and Git Editor saved history.
Every commit, checkout, revert, load, and merge path uses read (`capture`) and write (`apply`) here.

## Files

- `LevelKey.cpp`: level key string via required dep `cvolton.level-id-api`
- `LevelStateIO.cpp`: read level from the editor and write a saved version back

## Notes

Key format is `id:<editor id>`. With no level it returns `invalid:no-level`.
The pause hook rejects keys that start with `invalid:`.

`cvolton.level-id-api` is a required dependency in `mod.json`.

## Related

- [README.md](README.md)
- [service.md](service.md)
- [hooks.md](hooks.md)
- [model.md](model.md)
- [../building.md](../building.md)

## Source

- `src/editor/LevelKey.cpp`
- `src/editor/LevelStateIO.cpp`
