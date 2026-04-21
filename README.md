# Git Editor

Real diff-based history for the Geometry Dash editor. Every commit stores a
logical delta against its parent, so you can **revert a single commit**
without touching anything else.

## Features

- **Commit** - from the pause menu, name a snapshot. The mod parses the level,
  matches every object to a persistent UUID, and stores only what changed
  since the previous commit (add / remove / modify-per-field).
- **Checkout** - load any earlier commit's state into the editor. A new
  auto-revert commit is written on top of HEAD, so history is never lost.
- **Revert** - undo only the operations introduced by one specific commit.
  Later commits are preserved. Ops that can't be applied cleanly (object
  already gone, field drifted, etc.) are reported instead of silently
  clobbered.

All data is offline SQLite under the mod save dir (`git-editor.db`).

## Model

```
commit N  (parent = N-1)
   |
   v
commit 2  (parent = 1)
   |
   v
commit 1  (parent = null, full state as adds)
```

Each commit carries a JSON-serialized `Delta`:

```
{
  "h": { header field changes with before/after },
  "+": [ added objects    (uuid + fields) ],
  "-": [ removed objects  (uuid + fields) ],
  "~": [ modified objects (uuid + per-field before/after) ]
}
```

State at any commit is rebuilt by walking the parent chain from the root and
applying deltas. Recent reconstructions are cached in-memory (LRU).

## Object identity

GD doesn't give objects stable IDs, so the mod synthesizes one (64-bit
random) per object, persisted inside the committed delta.

On each new commit the matcher re-aligns UUIDs between the live level and
the previous HEAD:

1. Fingerprint match on `(type, rounded-x, rounded-y, rotation, groups)`.
2. Spatial nearest-neighbor fallback within 32 units, same type only.
3. Fresh UUID for anything still unmatched.

Consequence: nudging an object reads as a modify (position fields change),
not add+remove. Duplicating an object produces one modify and one add.

## Checkout vs. revert

- **Checkout = "jump to that state"** - fabricates a delta equal to
  `diff(HEAD, target)` and appends it as a new commit. HEAD always moves
  forward.
- **Revert = "undo just that commit"** - computes `diff(target, target.parent)`
  and applies it to HEAD with conflict reporting. The resulting delta is
  then re-derived from the actual before/after so the stored row is
  self-consistent even when some ops were skipped.

## Level identity

- Saved levels: keyed by `m_levelID`.
- Unsaved / local levels: keyed by `"name:<fnv1a64-hex>"`. Renaming forks
  history (by design).

## Known limitations

- No branches, merges, rebase. History is a single linear chain per level.
- Object identity across very large edits (many overlapping stacks, bulk
  rotations) can produce avoidable add+remove pairs rather than modifies.
  This is a cost-of-matching trade-off, not a correctness issue.
- Commit messages capped at 120 characters.
- SQLite schema version 2. Databases from pre-release snapshot builds are
  wiped automatically on first run.

## Build

```bash
cmake -B build
cmake --build build
```

Requires `GEODE_SDK` in the environment. SQLite is fetched by CPM.
