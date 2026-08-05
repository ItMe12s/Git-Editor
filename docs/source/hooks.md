# hooks

## Summary

Where the mod plugs into Geometry Dash. Adds Git Editor to the editor pause menu.
Opens UI popups and runs import/export jobs through `GitService`.

## Files

- `EditorPauseLayerHook.cpp`: Commit, History, Levels, Import, Export buttons and file flows
- `ImportGdgeFlow.cpp`: GDGE import plan popup, prepare/apply/finalize merge flow

## Notes

Player-facing button labels and popup flow live in [features/README.md](../features/README.md).

## Related

- [README.md](README.md)
- [ui.md](ui.md)
- [service.md](service.md)
- [../features/README.md](../features/README.md)
- [settings.md](settings.md)

## Source

- `src/hooks/EditorPauseLayerHook.cpp`
- `src/hooks/ImportGdgeFlow.cpp`
