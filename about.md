# Git Editor

A real diff-based history for the editor.
Every commit stores a delta against its parent,
so you can revert a change without reverting the whole level.

## Features

All of these are in the editor pause menu at the top :3

- **Commit**: name and save your changes into a delta snapshot in the history.
- **History**: a list of all commits for the level you're in.
  - **Revert**: undo a commit without undoing other ones.
  - **Checkout**: load a commit's state into the editor, you can revert this after you're done *checking out.* heh.
- **Levels**: a list of all levels with a commit history, you can delete the history of each level via this menu.

Everything is saved offline as a SQLite database inside the mod save folder (`git-editor.db`).

## Current limits

- No branches, merges or rebasing yet...
  - You can checkout -> copy objects -> revert checkout -> paste objects -> commit.
- Object identity across very large edits (overlapping stacks, bulk rotations) can make add+remove pairs instead of modifies.
  - This is a cost-of-matching trade-off, not a correctness issue.
- Commit messages capped at 120 characters.
- Databases using an old format will be wiped if the mod updates with a new one (your level won't be deleted).
