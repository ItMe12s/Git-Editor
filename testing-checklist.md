# Regression Checklist

Something will eventually break. I can feel it.

## Tags

- **(Automated)**: Covered by the in-game test (Mod Settings -> Run Automated Test). Checks the service/DB unless noted.
- **(Automated, UI manual)**: Service is tested. You still need to check the editor or popups by hand.
- **(Manual)**: Not run by the automated suite.

## Two-phase commit (prepare / finalize)

1. **(Automated)** `prepareCheckout` does not change HEAD or commit rows until `finalizeCheckout`.
2. **(Automated)** Checkout when HEAD already equals the target has no pending payload.
3. **(Automated)** `prepareImportManyFromGdge` on an empty level does not write the DB until finalize. Final state matches the payload.

## Checkout

1. **(Automated)** Create 3 commits. Checkout commit 1 from HEAD.
2. **(Automated, UI manual)** New commit is appended, HEAD updates, state matches commit 1. Automated test uses `hashLevelState`, not the live editor.
3. **(Automated)** Checkout again from the new commit back to the original HEAD state.
4. **(Automated)** Double-checkout chain has no state drift.

## Revert

1. **(Automated)** Revert the middle commit in a chain of 3 or more.
2. **(Automated)** Revert commit is appended. Later commits stay in the list.
3. **(Automated)** Conflict summary only when conflicts exist. Checks the `conflicts` list. **(Manual)** Check the popup.
4. **(Automated)** Revert a revert. Double-revert gives the expected state.
5. **(Automated)** Revert with overlapping edits gives non-empty conflicts.
6. **(Manual)** Revert on a level with 20+ commits. Correct and no lag.

## Squash

1. **(Automated)** Squash a contiguous range with a custom message.
2. **(Automated)** One commit replaces the range. HEAD remaps correctly.
3. **(Automated)** Squash full history to one commit. State matches original HEAD.
4. **(Automated)** Squash, then revert the squash commit. History stays valid.

## Import/Export, raw

1. **(Automated)** Turn off "Compress Export Files". Export a level.
2. **(Automated)** File starts with `SQLite format 3`.
3. **(Automated, UI manual)** Import into a clean level. Counts and state hash are automated. **(Manual)** Check the plan popup.

## Import/Export, compressed

1. **(Automated)** Turn on "Compress Export Files" (default). Export a level.
2. **(Automated)** File starts with `PK` bytes (rename to `.zip` to double-check).
3. **(Automated)** Import back. State matches the export. Compares payload hash to reconstructed HEAD on mixed import.

## Import mixed batch

1. **(Automated)** Export one level compressed and one raw.
2. **(Automated)** Import both in one session.
3. **(Automated, UI manual)** Plan accepts both files. Merged state is correct. Hash and chain length are automated. **(Manual)** Check the popup.

## Load level history

1. **(Automated)** Source level has several commits. Load into destination.
2. **(Automated)** Destination history matches source order and messages.
3. **(Manual)** Mix with import/export for multi-person collab from one file.

## Collab simulation

1. **(Automated)** Create layout, squash to one commit, export `.gdge`. Base is squashed before export.
2. **(Automated)** Two decorator levels: import base, commit, export.
3. **(Automated)** Import both decorator files on the layout level at once.
4. **(Automated)** Smart merge runs (same root). No conflicts when edits do not overlap.
5. **(Automated)** Overlapping edits: conflict count > 0, state still usable.

## Advanced collab simulation

1. **(Automated)** Triple smart merge (alice, bob, scratch). Bogus file is skipped.
2. **(Automated)** Smart vs sequential (shared root vs legacy export).
3. **(Automated)** Sequential import of foreign-root `.gdge`.
4. **(Automated)** Overlapping decorator merge: `conflictCount > 0`.

## Edge cases

1. **(Automated)** Import `.gdge` with a different root. Uses sequential merge, not smart.
2. **(Automated)** First commit on a level with no history. Root and reconstruction work.
3. **(Automated)** Delete level history. Key gone from `listLevels`, new commit works. **(Manual)** Check the Levels UI list.
4. **(Automated)** `updateMessage` shows up in `listSummaries`.
5. **(Automated)** `planImport` sets `noLocalCommits` when dest has no HEAD.
6. **(Manual)** History on 50+ commits: scroll, tap, edit, no lag.

## Geode index / paths / UI ids

1. **(Manual)** **Save dir**: First launch. No `createDirectoryAll` or DB errors in log. DB opens under mod save dir.
2. **(Automated)** **Paths**: Export/import `.gdge` with compress on and off. Use a non-ASCII path if you can.
3. **(Manual)** **Node IDs (optional)**: With `geode.node-ids`, check ids like `imes.git-editor/git-editor-history-scroll`, `imes.git-editor/git-editor-levels-scroll`, `imes.git-editor/git-editor-delta-scroll`, `imes.git-editor/git-editor-commit-message-input`.
