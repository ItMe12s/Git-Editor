# Git Editor

A mod that tries to implement git into the level editor

## Core model

- Linear history per level key, no branches.
- Commit stores JSON `Delta` against parent in `git-editor.db`.
- Delta keys: `h` (header), `+` (adds), `-` (removes), `~` (modifies).
- State reconstruction replays root -> HEAD, with LRU cache (default 16 states).

## Other info not in about.md

- Checkout is forward-only: inserts new commit with `diff(HEAD, target)`.
- Revert applies `diff(target, target.parent)` onto current HEAD, reports conflicts.
- Squash requires 2+ contiguous selected commits.

## Identity + keys

- Object UUID assignment uses fingerprint + nearest match (32-unit radius, same type).
- Saved level key: `m_levelID`.
- Unsaved observed keys are mapped to stable canonical aliases (`localid:<n>`) for persistent history.

## TO-DO

- Actually none, waiting for bug reports rn.

## Build

- SQLite via CPM in `CMakeLists.txt`.
- Manual testing: `testing-checklist.md`.
