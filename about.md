# <c-FFF9C4>G</c><c-FFF59D>i</c><c-FFF176>t</c> <c-FFEE58>E</c><c-FFEB3B>d</c><c-FDD835>i</c><c-FBC02D>t</c><c-F9A825>o</c><c-F57F17>r</c>

**A real diff-based history for the editor,
so you can undo a change without reverting the whole level.**

**DON'T FORGET TO CHECK CHANGELOG!!!**

---

## Features

**All of these are in the editor pause menu at the top :3**

### Commit

Name and save your changes into a delta snapshot in the history.
I recommend committing small changes like adding deco, triggers, buffing a section instead of doing one big commit.

And make sure to assign the intended *z-order* for your overlapping objects,
this mod does NOT care what objects are placed before or after each other.

### History

A list of all commits for the level you're in.
Long histories may briefly show **Loading commits...** while the list prepares.

- **Revert**: undo a commit without undoing other ones.
- **Checkout**: load a commit's state into the editor, you can revert this after you're done *checking out.* Heh.
- **Squash**: combine adjacent commits into a single commit.

### Levels

A list of all levels with a commit history.
Having a lot of levels can show **Loading levels...** for a moment.

- **Load**: overwrite the current level data with the data and history from another level.
- **Delete**: permanently delete the history of the selected level.

### Merge

Combine multiple levels (databases) into one.

- **.gdge**: it stands for Geometry Dash Git Editor
- **Export/Import**: export current level history to `.gdge` and import the same `.gdge` but modified and it will try to use smart merge.
- **3-Way Merge**: the mod understands the base of your level and resolves conflicts for you. (does NOT fix bad collab hosts giving out incorrect color channel and group id).

Everything is saved offline as a SQLite database inside the mod save folder (`git-editor.db`). Exported `.gdge` files are compressed with zip by default to save disk space.

---

## Some Examples

- **Time travel**: you can checkout an earlier commit to go back in time, make a change, and commit it. Then you can revert the checkout to *rewrite history* with that new commit. Also squash them into one commit.
- **Backup mod**: keep a full history of your levels (Pro tip: You can add `git-editor.db` to a zip file so its *10%* of the size and you can put it anywhere like Google Drive or Discord).

---

## Current Limits

- **No branches, or rebasing yet**: tho you can checkout -> copy objects -> revert checkout -> paste objects -> commit, this is a manual merge.
- **Object identity**: large edits may show add+remove instead of modify, this isn't a bug.
- **Commit messages**: capped at 120 characters.
- **No auto migration**: you can *contact me* on Discord for help. This will not be an issue once it's out of beta.

---

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
