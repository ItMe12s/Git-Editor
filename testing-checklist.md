# Regression Checklist

Something will eventually break I can feel it.

## Checkout

1. Create 3 commits. Checkout commit 1 from HEAD. **(Included in automated test.)**
2. Verify new commit appended, HEAD updated, editor state matches commit 1. **(Included in automated test.)**
3. Checkout again from the new commit back to original HEAD state. **(Included in automated test.)**
4. Verify double-checkout chain has no state drift. **(Included in automated test.)**

## Revert

1. Revert middle commit in a 3+ commit chain. **(Included in automated test.)**
2. Verify revert commit appended, later commits intact. **(Included in automated test.)**
3. Verify conflict summary only appears when conflicts exist.
4. Revert a revert. Verify double-revert produces expected state.
5. Revert on a level with 20+ commits. Verify correctness and no lag.

## Squash

1. Squash a contiguous range with a custom message. **(Included in automated test.)**
2. Verify single commit replaces range, HEAD remaps correctly. **(Included in automated test.)**
3. Squash entire history to one commit. Verify squashed state matches original HEAD. **(Included in automated test.)**
4. Squash then revert the squash commit. Verify history integrity. **(Included in automated test.)**

## Import/Export, raw

1. Disable "Compress Export Files". Export a level. **(Included in automated test.)**
2. Verify file starts with `SQLite format 3` (hex check). **(Included in automated test.)**
3. Import back into a clean level. Verify plan popup and merge counts.

## Import/Export, compressed

1. Enable "Compress Export Files" (default). Export a level. **(Included in automated test.)**
2. Verify file starts with `PK` bytes (rename to `.zip` to confirm). **(Included in automated test.)**
3. Import back. Verify state matches exported level.

## Import mixed batch

1. Export one level compressed, one raw. **(Included in automated test.)**
2. Import both in one pick session. **(Included in automated test.)**
3. Verify plan popup accepts both and merged state is correct.

## Load level history

1. Create source level with multiple commits. Load into destination. **(Included in automated test.)**
2. Verify destination history replaced with source chain (order and messages match). **(Included in automated test.)**
3. Mix with import/export to simulate 2-3 person collab from a single file.

## Collab simulation

1. Create layout. Squash to one commit. Export as `.gdge`.
2. Two decorator levels: import base, make independent commits, export. **(Included in automated test.)**
3. Import both decorator files at once on the layout level. **(Included in automated test.)**
4. Verify smart merge fires (same root), no conflicts with non-overlapping edits.
5. Repeat with overlapping edits. Verify conflict count > 0, state still usable. **(Included in automated test.)**

## Edge cases

1. Import `.gdge` with a different root. Verify sequential merge, not smart. **(Included in automated test.)**
2. First commit on a level with no history. Verify root set and reconstruction works. **(Included in automated test.)**
3. Delete a level's history. Verify gone from Levels list, fresh commit works.
4. Open History on 50+ commit level. Verify scroll, tap, and edit have no lag.

## Geode index / paths / UI ids

1. **Save dir:** First launch, confirm no `createDirectoryAll` / DB open errors in log, DB under mod save dir opens.
2. **Paths:** Export/import a `.gdge` with compress on and off. If possible, use a path with non-ASCII characters to stress path handling. **(Included in automated test.)**
3. **Node IDs (optional dev check):** With `geode.node-ids` / dev tools, `querySelector` (or child-by-id) for ids such as `imes.git-editor/git-editor-history-scroll`, `imes.git-editor/git-editor-levels-scroll`, `imes.git-editor/git-editor-delta-scroll`, `imes.git-editor/git-editor-commit-message-input`.
