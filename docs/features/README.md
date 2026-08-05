# Pause menu map

## Summary

Open the editor pause menu. Git Editor buttons sit in two rows near the top.
Player details also live in the in-game About page.

## Buttons (top row)

- **Commit**: message popup to name and save a snapshot
- **History**: list of commits for this level (may show **Loading commits...**)
- **Levels**: all levels that have history (may show **Loading levels...**)

## Buttons (bottom row)

- **Import .gdge**: file picker, then import plan alert
- **Export .gdge**: file picker, then export

## Inside History

- **Revert**: undo one commit without undoing the rest
- **Checkout**: load that version into the editor and add a new snapshot on your timeline. To undo, revert that checkout snapshot
- **Squash Mode** then **Squash N**: pick adjacent commits, then combine them into one
- **? (changes)**: show what changed in that commit. Long diffs split into pages you can flip through
- **Rename**: change the commit message

Revert and merge may pop up a short alert listing objects that could not apply cleanly. Tap to dismiss and the rest of the change still goes through.

## Inside Levels

- **Load**: replace this level and its history with the selected level
- **Delete**: remove all history for that level (permanent)

Each row shows commit count, last commit time, and size on disk. The top of the popup shows the total database size.

## After picking Import .gdge

The mod scans the files and shows an **Import plan** popup. Files are sorted into:

- **Smart merge**: same starting point as your level, auto-merged in
- **Sequential**: different starting point, applied one commit at a time
- **Skipped**: file could not be read

Accept with **Merge** (or cancel). The mod then runs the merge in the background.

## Related

- [../../about.md](../../about.md)
- [../known-issues.md](../known-issues.md)
- [../source/hooks.md](../source/hooks.md)
- [../source/ui.md](../source/ui.md)
- [../source/settings.md](../source/settings.md)
- [../index.md](../index.md)

## Source

- `src/hooks/EditorPauseLayerHook.cpp`
- `src/hooks/ImportGdgeFlow.cpp`
- `src/ui/HistoryLayer.cpp`
- `src/ui/LevelBrowserLayer.cpp`
