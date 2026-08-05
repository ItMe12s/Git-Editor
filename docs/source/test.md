# test

## Summary

Automated checks compiled into the mod. Tests run inside the game, not as a separate program.
Uses real `git-editor.db` in save data. Same build as the shipped mod.

## Files

- `AutomatedTestRunner.cpp`: runs all suites and writes a report
- `AutomatedTestHarness.cpp`: shared helpers and report format
- `TwoPhaseTests.cpp`, `CheckoutTests.cpp`, `RevertTests.cpp`, `SquashTests.cpp`
- `GdgeImportExportTests.cpp`, `HistoryTests.cpp`, `CollabTests.cpp`
- `AdvancedCollabTests.cpp`, `EdgeTests.cpp`, `CommitPerfTests.cpp`
- `ManualChecklistTests.cpp`

## Notes

Open Geode mod settings and choose **Run Automated Test**. Output goes to `test-result.txt` in the mod save folder.
The settings handler writes that file. The harness only builds the report text.

Suites run in this order:

- TwoPhase
- Checkout
- Revert
- Squash
- ImportExport
- LoadLevelHistory
- Collab
- AdvancedCollab
- Edge
- Perf

ManualChecklist is skipped.

Pair with [testing-checklist.md](../../testing-checklist.md) after service or store changes.

## Related

- [README.md](README.md)
- [settings.md](settings.md)
- [service.md](service.md)
- [../../testing-checklist.md](../../testing-checklist.md)

## Source

- `src/test/AutomatedTestRunner.cpp`
- `src/test/AutomatedTestHarness.cpp`
- `src/test/CommitPerfTests.cpp`
- `src/settings/RunAutomatedTestSetting.cpp`
