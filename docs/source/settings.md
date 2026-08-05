# settings

## Summary

Code that runs when you change mod settings in Geode. Setting keys and display names live in `mod.json`.

## Files

- `RunAutomatedTestSetting.cpp`: runs in-mod test suites, writes `test-result.txt`

## Notes

| Key | Display name |
| --- | --- |
| `size-multiplier` | Top Row Button Size Multiplier |
| `compress-export-files` | Compress Export Files |
| `run-automated-test` | Run Automated Test |

Button size is read in `EditorPauseLayerHook`. Compress is read during export in store and service code.
Automated test details live in [test.md](test.md).

## Related

- [README.md](README.md)
- [test.md](test.md)
- [hooks.md](hooks.md)
- [../features/README.md](../features/README.md)
- [../../mod.json](../../mod.json)

## Source

- `mod.json`
- `src/settings/RunAutomatedTestSetting.cpp`
