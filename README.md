# Git Editor

Linear, diff-based version history for the Geometry Dash editor. Each commit stores a JSON `Delta` against its parent, state is **replay** from root + apply chain. Data is local SQLite: `git-editor.db` in the mod save directory.

## Capabilities (pause menu, top center)

- **`Commit`** - Parse live level, match objects to stable UUIDs, append delta vs previous HEAD: `+` / `-` / `~` / header `h`.
- **`Checkout` (via History)** - Append commit whose delta equals `diff(HEAD, target)`, **HEAD only moves forward**, never destructive rewind.
- **`Revert` (on a commit in History)** - Compute `diff(target, target.parent)` against current state, report conflicts, stored delta re-derived from actual before/after if some ops skip.
- **`Levels`** - Enumerate level keys in DB, **delete history** for a key only. No checkout/revert from this screen.

Row: **Commit** , **History** , **Levels**. History lists commits for the **current** level, synthetic checkout/revert entries labeled in small text (no separate badge row).

## Storage and replay

Linear chain: `commit_N -> ... -> commit_1`, `commit_1` parent `null` (full state represented as adds).

`Delta` shape (JSON keys):

- `h` - level header: field before/after.
- `+` / `-` - objects added or removed (uuid + fields).
- `~` - per-object field before/after.

Reconstruction: walk from root, apply deltas. **LRU** cache of recent full-state builds.

## Object identity

Editor objects have no native stable id. Mod assigns **64-bit random** UUID per object, stored in committed deltas. On each commit, matcher maps live level <-> previous HEAD:

1. Fingerprint: `(type, rounded-x, rounded-y, rotation, groups)`.
2. Nearest neighbor within **32** units, same type only.
3. Unmatched → new UUID.

Effect: small moves are `~` (field changes), not add+remove. Duplicating yields one `~` and one `+`.

## Checkout vs revert

- **Checkout** - New commit, delta = `diff(HEAD, target)`.
- **Revert (single commit)** - Apply `diff(target, target.parent)` to HEAD; see conflict / re-derive behavior above.

## Level keys

- Saved: `m_levelID`.
- Local/unsaved: `name:<fnv1a64-hex>`. **Rename = new key** = forked history.

## Build

SQLite is fetched by CPM.
