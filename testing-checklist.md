# Regression Checklist

Something will eventually break I can feel it.

## Checkout

1. Create 3 commits on one level.
2. Checkout commit 1 from latest HEAD.
3. Verify new commit is appended, HEAD changes, editor state matches commit 1.
4. Verify History list still opens and commit messages remain editable.

## Revert

1. Revert middle commit in a 3+ commit chain.
2. Verify revert commit is appended and later commits remain.
3. Verify conflict summary appears only when conflicts exist.

## Squash

1. Enter squash mode and select contiguous commits.
2. Squash with custom message.
3. Verify one new commit replaces selected range, HEAD remaps correctly.
4. Verify list refresh and state application still work.

## Import/Export

1. Export a level to `.gdge`.
2. Import exported file back into a clean level.
3. Verify smart/sequential bucket selection appears in import plan popup.
4. Verify merged counts, skipped counts, and conflict counts appear in final notification.

## Load level history copy

1. Create source level with multiple commits.
2. Load source history into destination level.
3. Verify destination old history is removed and source chain copied with matching order/messages.
4. Mix this with import/export to test real collabs scenarios. Like at least 2-3 people (levels from a single file).
