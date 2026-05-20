# hooks

Where the mod plugs into Geometry Dash. Adds Git Editor to the editor pause menu.

## Main files

- `EditorPauseLayerHook.cpp`: Commit, History, Levels, Import, Export buttons and file flows

## Touches

Opens UI popups. Shows import preview dialogs and runs import/export jobs through `GitService`.

## You might care if

You want to find where pause menu buttons are wired. Player details are in [features/README.md](../features/README.md).

## Code

- [src/hooks/EditorPauseLayerHook.cpp](../../src/hooks/EditorPauseLayerHook.cpp)
