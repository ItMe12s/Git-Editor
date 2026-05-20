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
- **? (changes)**: show what changed in that commit
- **Rename**: change the commit message

## Inside Levels

- **Load**: replace this level and its history with the selected level
- **Delete**: remove all history for that level (permanent)

## Related

- [source/settings.md](../source/settings.md): button size, zip exports, automated test
- [source/hooks.md](../source/hooks.md): where pause menu buttons are wired
- [source/ui.md](../source/ui.md): History and Levels popups
