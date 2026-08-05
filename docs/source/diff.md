# diff

## Summary

Compares two saved level versions and stores only the changes. Replays those changes to rebuild history.
`GitService` commits and rebuilds state. History UI shows text from changes via `DeltaText`.

## Files

- `Delta.hpp` / `Delta.cpp`: change data and JSON save/load (`dumpDelta` / `parseDelta`)
- `Differ.hpp` / `Differ.cpp`: `diff`, `apply`, and conflict list

## Notes

- `h`: header changes
- `hr`: raw header
- `+`: new objects
- `-`: removed objects
- `~`: changed objects
- Each level has one straight timeline of saves. No split timelines
- Conflicts come up when an object is already gone, never existed, or has moved on from the version the change expected.
  Revert shows these in a short alert
- There is no `undo` API. Revert is `diff` plus `apply` inside `GitService::prepareRevert`

## Related

- [README.md](README.md)
- [model.md](model.md)
- [identity.md](identity.md)
- [service.md](service.md)
- [ui.md](ui.md)
- [../known-issues.md](../known-issues.md)

## Source

- `src/diff/Delta.hpp`
- `src/diff/Delta.cpp`
- `src/diff/Differ.hpp`
- `src/diff/Differ.cpp`
