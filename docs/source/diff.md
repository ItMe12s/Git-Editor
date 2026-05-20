# diff

Compares two saved level versions and stores only the changes. Replays those changes to rebuild history.

## Main files

- `Delta.cpp`: change data and JSON save/load
- `Differ.cpp`: compare, apply, undo a change set, conflict list

## Notes

- `h`: header changes
- `+`: new objects
- `-`: removed objects
- `~`: changed objects
- Each level has one straight timeline of saves. No split timelines.

## Touches

`GitService` commits and rebuilds state. History UI shows text from changes via `DeltaText`.

## You might care if

Revert shows a conflict summary. Checkout always saves a new snapshot instead of jumping back silently.

## Code

- [src/diff/Delta.hpp](../../src/diff/Delta.hpp)
- [src/diff/Differ.cpp](../../src/diff/Differ.cpp)
