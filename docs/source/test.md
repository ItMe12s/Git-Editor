# test

Automated checks compiled into the mod. Tests run inside the game, not as a separate program.

## Main files

- `AutomatedTestRunner.cpp`: runs all suites and writes a report
- `AutomatedTestHarness.cpp`: shared helpers and report format
- `TwoPhaseTests.cpp`, `CheckoutTests.cpp`, `RevertTests.cpp`, `SquashTests.cpp`
- `GdgeImportExportTests.cpp`, `HistoryTests.cpp`, `CollabTests.cpp`
- `AdvancedCollabTests.cpp`, `EdgeTests.cpp`, `ManualChecklistTests.cpp`

## Notes

Open Geode mod settings and choose **Run Automated Test**. Output goes to `test-result.txt` in the mod save folder.

Suites run in order: TwoPhase, Checkout, Revert, Squash, ImportExport, LoadLevelHistory, Collab, AdvancedCollab, Edge, then ManualChecklist skips.

## Touches

Uses real `git-editor.db` in save data. Same build as the shipped mod.

## You might care if

You change service or store code. Pair with [testing-checklist.md](../../testing-checklist.md).

## Code

- [src/test/AutomatedTestRunner.cpp](../../src/test/AutomatedTestRunner.cpp)
- [testing-checklist.md](../../testing-checklist.md)
