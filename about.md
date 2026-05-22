# <c-FFF9C4>G</c><c-FFF59D>i</c><c-FFF176>t</c> <c-FFEE58>E</c><c-FFEB3B>d</c><c-FDD835>i</c><c-FBC02D>t</c><c-F9A825>o</c><c-F57F17>r</c>

**A real diff-based history for the editor, so you can undo a change without reverting the whole level.**

**DON'T FORGET TO CHECK THE CHANGELOG!!!**

Collab guide at the bottom!

---

## Features

**All of these can be found in the editor pause menu at the top :3**

### Commit

Name and save your changes into a delta snapshot in the history.

I recommend committing small changes like adding decoration, trigger changes, or buffing a section instead of doing one massive commit.

*Note: Make sure to assign the intended z-order for your overlapping objects.*
*This mod does NOT care about the order in which objects were placed.*

### History

A list of all commits for the level you're currently in.

Long histories may briefly show **Loading commits...** while the list prepares.

- **Revert:** Undo a specific commit without rolling back any of the changes made after it.
- **Checkout:** Load a past commit's state into the editor. You can safely revert this when you're done *checking out.* Heh.
- **Squash:** Combine adjacent commits into a single, clean commit.

### Levels

A list of all levels that have an active commit history.

If you have a ton of levels, it might show **Loading levels...** for a moment.

- **Load:** Overwrite the current level data with the data and history from another level.
- **Delete:** Permanently delete the history of the selected level.

### Merge

Combine multiple level databases into one.

- **.gdge:** This stands for *Geometry Dash Git Editor*.
- **Export/Import:** Export your current level history to a `.gdge` file, or import a modified `.gdge` file to trigger a smart merge.
- **3-Way Merge:** The mod analyzes the original base of your level and automatically resolves object conflicts for you. *(Does NOT fix bad collab hosts giving out incorrect color channels and group IDs... yet).*

**Note:** Everything is saved offline as a SQLite database inside the mod's save folder (`git-editor.db`).
Exported `.gdge` files are automatically compressed into ZIP format to save disk space.

---

## Some Examples of What You Can Do

- **Time Travel:** You can checkout an earlier commit to go back in time, make a change, and commit it. Then, revert the checkout to *rewrite history* with your new commit. You can also squash them later to keep things tidy.
- **Backup System:** Keep a complete, uncorrupted history of your levels.

*Pro tip: You can manually add `git-editor.db` to a ZIP file to compress it to 10% of its size,*
*making it easy to back up to Google Drive or Discord.*

---

## Current Limits

- **No branches or rebasing yet:** Though you can manually merge by using checkout -> copy objects -> revert checkout -> paste objects -> commit.
- **Object Identity:** Large or sweeping edits may show up as an "add + remove" instead of a "modify." This is normal behavior, not a bug.
- **Commit Messages:** Strictly capped at 120 characters.

If you ever need help or a custom migration script, feel free to *contact me* on Discord!

---

## Collab Guide

1. Create your base layout.
2. Commit and squash everything into a single layout commit.
3. Export the `.gdge` file and send it to your decorators.
4. Decorators do their work and commit their specific changes.
5. Have your decorators export their finished work back to `.gdge` files.
6. Click import `.gdge` (importing all of them at once should work).
7. ???
8. Profit!

*Note: Your layout acts as the base. When you import the decorators' files,*
*all of their unique commits will cleanly apply directly onto your layout.*
*If you don't send the original layout as a `.gdge` file first, the smart merge system won't know how to sync them.*
