# Git Editor

CHECK CHANGELOG BEFORE UPDATING!!! Read at the bottom for collab guide.

A real diff-based history for the editor.
Every commit stores a delta against its parent,
so you can revert a change without reverting the whole level.

## Features

All of these are in the editor pause menu at the top :3

### Commit: name and save your changes into a delta snapshot in the history

### History: a list of all commits for the level you're in

- **Revert**: undo a commit without undoing other ones.
- **Checkout**: load a commit's state into the editor, you can revert this after you're done *checking out.* Heh.
- **Squash**: combine adjacent commits into a single commit.

### Levels: a list of all levels with a commit history

- **Load**: overwrite the current level data with the data and history from another level.
- **Delete**: permanently delete the history of the selected level.

### Merge: combine multiple databases (levels)

- **.gdge**: Geometry Dash Git Editor mod's custom file format.
- **Export/Import**: export current level history to `.gdge` and import the same `.gdge` but modified and it will try to use smart merge.
- **3-Way Merge**: the mod understands the base of your level and resolves conflicts for you. (does NOT fix bad collab hosts giving out incorrect color channel and group id).

Everything is saved offline as a SQLite database inside the mod save folder (`git-editor.db`). Exported `.gdge` files are compressed with zip by default to save disk space, which can be toggled in the mod settings under "Compress Export Files". Imported `.gdge` files are detected automatically, both compressed and uncompressed work.

## Some Examples

- **Time travel**: you can checkout an earlier commit to go back in time, make a change, and commit it. Then you can revert the checkout to *rewrite history* with that new commit. Also squash them into one commit.
- **Backup mod**: keep a full history of your levels while using significantly less disk space long term.

## Current Limits

- **No branches, or rebasing yet...**
  - Tho you can checkout -> copy objects -> revert checkout -> paste objects -> commit, this is a manual merge.
- **Object identity**: across very large edits (overlapping stacks, bulk rotations) may create add+remove pairs instead of modifies.
  - This is a cost-of-matching trade-off, not a correctness issue.
- **Commit messages**: capped at 120 characters.
- **Breaking updates**: databases using an old format will be wiped if the mod updates with a new one (your actual level won't be deleted). So make sure to backup your database file first, this rarely happens unless there's a new massive feature. This is because I don't wanna make a migration script.

## Collab Guide

1. Make a layout.
2. Commit/Squash everything into one commit.
3. Export the .gdge file and send it to your decorators.
4. Decorator work and commit their changes.
5. Have your decorators export and send back the .gdge files.
6. Click import .gdge (all of them at once should work) and pray.
7. ???
8. Profit!

Your layout will act as a base and when you import the files, all of the commits will apply to your layout.
If you don't have the layout sent as a .gdge file then smart merge won't work.
