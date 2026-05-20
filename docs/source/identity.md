# identity

Gives each object a stable ID so the change tracker can tell edits apart from delete-plus-add.

Geometry Dash does not export real object IDs. This folder matches objects between saves by position and type.

## Main files

- `Matcher.cpp`: assigns stable IDs to objects (`assignUuids`, `assignFreshUuids`)

## Touches

Runs on commit when `GitService` captures the level.

## You might care if

A huge reshape shows many adds and removes instead of edits. That is normal, not a bug.

## Code

- [src/identity/Matcher.hpp](../../src/identity/Matcher.hpp)
- [src/identity/Matcher.cpp](../../src/identity/Matcher.cpp)
