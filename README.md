# Git Editor

A mod that tries to implement git into the level editor

## Core model

- Linear history per level key, no branches.
- Commit stores JSON `Delta` against parent in `git-editor.db`.
- Delta keys: `h` (header), `+` (adds), `-` (removes), `~` (modifies).
- State reconstruction replays root -> HEAD, with LRU cache (default 16 states).

## Editor UI (pause menu top row)

- `Commit`: parse live level, UUID-match objects, store delta, message max 120 chars.
- `History`: inspect (`?`), rename, checkout, revert, squash.
- `Levels`: list level histories, load selected history into current level, or delete selected level history.

## Semantics

- Checkout is forward-only: inserts new commit with `diff(HEAD, target)`.
- Revert applies `diff(target, target.parent)` onto current HEAD, reports conflicts.
- Squash requires 2+ contiguous selected commits.
- Load is destructive for current level objects/history (explicit warning in UI).

## Identity + keys

- Object UUID assignment uses fingerprint + nearest match (32-unit radius, same type).
- Saved level key: `m_levelID`.
- Unsaved level key: `name:<fnv1a64-hex>` (rename => new key).

## Limits / ops notes

- No merge/rebase/branch flow.
- Large edits can degrade matching into add/remove pairs.
- Schema version bump can wipe commit DB data (not GD level files).

## Build

- SQLite via CPM in `CMakeLists.txt`.
