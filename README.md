# Git Editor

Per-level commit history for the Geometry Dash editor, inspired by `git`.
Commits store a snapshot of the entire level string locally, so you can freely
experiment and roll back to any earlier state.

## Features (core demo)

- **Commit** - from the editor pause menu, write a short message and snapshot
  the level's current state.
- **History** - browse every commit for the current level (newest first) with
  timestamp + message.
- **Checkout** - restore any earlier commit. This performs a **full replace**
  of the level's objects with the selected snapshot.

All data is stored offline in a SQLite database under the mod's save directory
(`git-editor.db`). Nothing is uploaded anywhere.

## Usage

1. Open any level in the editor.
2. Open the pause menu (Esc / pause button).
3. Use the two buttons on the left edge:
   - **Commit** - type a message, confirm.
   - **History** - pick a commit, press **Checkout**, confirm the replace.

## Level identity

- Saved levels are keyed by their numeric level id.
- Unsaved / local levels are keyed by an FNV-1a hash of the level name, so
  renaming an in-progress level will fork its history (by design).

## Known limitations

- **Checkout is destructive**: it calls `removeAllObjects()` and rebuilds the
  level from the stored string. The editor's own undo/redo stack is **not**
  rewound; use the editor's built-in undo only for in-session changes.
- No remote, push, pull, branch, or merge - this is a local linear history.
- Commit messages are capped at 120 characters.
- Each commit stores the full level string, so storage scales with level size
  times commit count.

## Build

Standard Geode mod build. Requires the `GEODE_SDK` environment variable.
SQLite is pulled in via CPM at configure time (see `CMakeLists.txt`).

```bash
cmake -B build
cmake --build build
```
