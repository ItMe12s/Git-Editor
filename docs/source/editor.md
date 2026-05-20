# editor

Bridge between the Geometry Dash editor and Git Editor saved history.

## Main files

- `LevelKey.cpp`: level key string (for example `id:123`) via **cvolton.level-id-api**
- `LevelStateIO.cpp`: read level from the editor and write a saved version back

## Touches

Every commit, checkout, revert, load, and merge path uses read (`capture`) and write (`apply`) here.

## You might care if

History is missing for a level. Install the Level ID API mod so each level keeps the same ID across sessions.

## Code

- [src/editor/LevelKey.cpp](../../src/editor/LevelKey.cpp)
- [src/editor/LevelStateIO.cpp](../../src/editor/LevelStateIO.cpp)
