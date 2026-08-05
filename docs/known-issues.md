# Known issues

## Summary

Limits and quirks to know before you play or update.
For version-specific wipe warnings, read the changelog.

## Caps

- No branches or rebasing. Each level has one straight timeline
- Commit messages are capped at 120 characters in the UI
- Large or sweeping edits may show as add plus remove instead of modify. See [source/identity.md](source/identity.md)

## While you play

- History and Levels lists may lag or freeze briefly while saving

## Exports and imports

- Failed exports may leave temp files (`.tmp` or `.sqlite-tmp`). See [changelog.md](../changelog.md)
- Some updates cannot read very old `.gdge` files. See [changelog.md](../changelog.md)

## Database upgrades

- Some beta versions wipe `git-editor.db` when you update. Read [changelog.md](../changelog.md) first
- There is no tool yet to move old save data to new versions while the mod is in beta

## For developers

- The code expects the database to enforce linked-table rules (SQLite foreign keys on)
- Reading history on screen while saving in the background can sometimes clash

## Related

- [index.md](index.md)
- [../changelog.md](../changelog.md)
- [features/README.md](features/README.md)
- [source/identity.md](source/identity.md)
- [source/store.md](source/store.md)

## Source

- `src/ui/CommitMessageLayer.hpp`
- `src/identity/Matcher.cpp`
- `src/store/CommitStore.cpp`
