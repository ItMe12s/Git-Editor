# Known issues

Limits and quirks to know before you play or update.

## See also

- [docs/index.md](index.md): all docs
- [changelog.md](../changelog.md): version notes and save wipe warnings

## While you play

- History and Levels lists may lag or freeze briefly while saving
- Big edits may look like many objects were deleted and re-added, not edited. See [source/identity.md](source/identity.md)

## Exports and imports

- Failed exports may leave temp files (`.tmp` or `.sqlite-tmp`). See [changelog.md](../changelog.md)
- Some updates cannot read very old `.gdge` files. See [changelog.md](../changelog.md)

## Database upgrades

- Some beta versions wipe `git-editor.db` when you update. Read [changelog.md](../changelog.md) first
- There is no tool yet to move old save data to new versions while the mod is in beta

## For developers

- The code expects the database to enforce linked-table rules (SQLite foreign keys on)
- Reading history on screen while saving in the background can sometimes clash
