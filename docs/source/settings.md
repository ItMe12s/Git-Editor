# settings

Code that runs when you change mod settings in Geode.

## Main files

- `RunAutomatedTestSetting.cpp`: runs in-mod test suites, writes `test-result.txt`

## Notes

- Top Row Button Size Multiplier: scales pause menu Git buttons
- Compress Export Files: zip `.gdge` exports when on
- Run Automated Test: developer check. See [test.md](test.md)

## Touches

Button size is read in `EditorPauseLayerHook`. Compress is read during export in store and service code.

## You might care if

Buttons feel too small or exports should be smaller on disk.

## Code

- [mod.json](../../mod.json)
- [src/settings/RunAutomatedTestSetting.cpp](../../src/settings/RunAutomatedTestSetting.cpp)
