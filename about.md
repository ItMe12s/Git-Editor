# Git Editor

CHECK CHANGELOG BEFORE UPDATING!!!

A real diff-based history for the editor.
Every commit stores a delta against its parent,
so you can revert a change without reverting the whole level.

## Features

All of these are in the editor pause menu at the top :3

### Commit: name and save your changes into a delta snapshot in the history

### History: a list of all commits for the level you're in

- **Revert**: undo a commit without undoing other ones.
- **Checkout**: load a commit's state into the editor, you can revert this after you're done *checking out.* Heh.
- **Squash**: Combine adjacent commits into a single commit.

### Levels: a list of all levels with a commit history

- **Load**: overwrite the current level data with the data and history from another level.
- **Delete**: permanently delete the history of the selected level.

### Merge: (WIP) combine multiple databases with the same initial commit for a single level to automatically merge collab parts

Everything is saved offline as a SQLite database inside the mod save folder (`git-editor.db`).

## Some Examples

- **Time travel**: you can checkout an earlier commit to go back in time, make a change, and commit it. Then you can revert the checkout to *rewrite history* with that new commit. Also squash them into one commit.
- **Backup mod**: keep a full history of your levels while using significantly less disk space long term.

## Current Limits

- **No branches, merges or rebasing yet...**
  - Tho you can checkout -> copy objects -> revert checkout -> paste objects -> commit.
- **Object identity**: across very large edits (overlapping stacks, bulk rotations) may create add+remove pairs instead of modifies.
  - This is a cost-of-matching trade-off, not a correctness issue.
- **Commit messages**: capped at 120 characters.
- **Breaking updates**: databases using an old format will be wiped if the mod updates with a new one (your actual level won't be deleted). So make sure to backup your database file first, this rarely happens unless there's a new massive feature.
