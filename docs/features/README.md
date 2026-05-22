# Pause menu map

Open the editor pause menu. Git Editor buttons sit in **two rows** near the top.

## See also

- [docs/index.md](../index.md): all docs
- [about.md](../../about.md): full player guide (also in-game)
  - **Features**: Commit, History, Levels, Merge
  - **Some Examples**: time travel and backup tips
  - **Current Limits**: branches, object identity, message length
  - **Collab Guide**: `.gdge` workflow

**Merge** in about is not its own button. Use **Import .gdge** (file picker, import plan, then merge).

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
- **Squash Mode** then **Squash N**: pick **adjacent** commits, then combine them into one
- **? (changes)**: show what changed in that commit. Long diffs split into pages you can flip through
- **Rename**: change the commit message

Revert and merge may pop up a short alert listing objects that could not apply cleanly. Tap to dismiss and the rest of the change still goes through.

## Inside Levels

- **Load**: replace this level and its history with the selected level
- **Delete**: remove all history for that level (permanent)

Each row shows commit count, last commit time, and size on disk. The top of the popup shows the total database size.

## After picking Import .gdge

The mod scans the files and shows an import plan popup. Files are sorted into:

- **Smart merge**: same starting point as your level, auto-merged in
- **Sequential**: different starting point, applied one commit at a time
- **Skipped**: file could not be read

Accept the plan and the mod runs the merge in the background.

## Related

- [source/settings.md](../source/settings.md): button size, zip exports, automated test
- [source/hooks.md](../source/hooks.md): where pause menu buttons are wired
- [source/ui.md](../source/ui.md): History and Levels popups
