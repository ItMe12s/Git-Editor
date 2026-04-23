# Regression Checklist

Something will eventually break I can feel it.

## Checkout

1. Create 3 commits. Checkout commit 1 from HEAD.
2. Verify new commit appended, HEAD updated, editor state matches commit 1.
3. Checkout again from the new commit back to original HEAD state.
4. Verify double-checkout chain has no state drift.

## Revert

1. Revert middle commit in a 3+ commit chain.
2. Verify revert commit appended, later commits intact.
3. Verify conflict summary only appears when conflicts exist.
4. Revert a revert. Verify double-revert produces expected state.
5. Revert on a level with 20+ commits. Verify correctness and no lag.

## Squash

1. Squash a contiguous range with a custom message.
2. Verify single commit replaces range, HEAD remaps correctly.
3. Squash entire history to one commit. Verify squashed state matches original HEAD.
4. Squash then revert the squash commit. Verify history integrity.

## Import/Export, raw

1. Disable "Compress Export Files". Export a level.
2. Verify file starts with `SQLite format 3` (hex check).
3. Import back into a clean level. Verify plan popup and merge counts.

## Import/Export, compressed

1. Enable "Compress Export Files" (default). Export a level.
2. Verify file starts with `PK` bytes (rename to `.zip` to confirm).
3. Import back. Verify state matches exported level.

## Import mixed batch

1. Export one level compressed, one raw.
2. Import both in one pick session.
3. Verify plan popup accepts both and merged state is correct.

## Local database compression

1. Enable "Compress Local Database" (default). Commit. Exit GD cleanly.
2. Verify `git-editor.db.zip` exists, no raw `.db` remains.
3. Restart. Verify history intact.
4. Disable setting. Restart. Verify only raw `.db` exists, zip removed.
5. Re-enable. Commit. Exit. Verify zip reappears.
6. Kill GD mid-session. Verify raw `.db` survives and history loads on next startup.

## Load level history

1. Create source level with multiple commits. Load into destination.
2. Verify destination history replaced with source chain (order and messages match).
3. Mix with import/export to simulate 2-3 person collab from a single file.

## Collab simulation

1. Create layout. Squash to one commit. Export as `.gdge`.
2. Two decorator levels: import base, make independent commits, export.
3. Import both decorator files at once on the layout level.
4. Verify smart merge fires (same root), no conflicts with non-overlapping edits.
5. Repeat with overlapping edits. Verify conflict count > 0, state still usable.

## Edge cases

1. Import `.gdge` with a different root. Verify sequential merge, not smart.
2. First commit on a level with no history. Verify root set and reconstruction works.
3. Delete a level's history. Verify gone from Levels list, fresh commit works.
4. Open History on 50+ commit level. Verify scroll, tap, and edit have no lag.
