# Source folder map

Code lives under `src/`. Menus talk to **service/**. Service talks to **store/** for saved files. Slow work uses **util/** on a background thread.

## Flow

- `hooks/` opens `ui/` popups
- `ui/` calls `service/` and `editor/`
- `ui/presentation/` and `ui/common/` hold labels and background UI helpers
- `service/` uses `store/`, `model/`, `diff/`, `identity/`, and `core/`
- `diff/` and `identity/` both use `model/`
- `util/io/` handles files, zip, and blobs
- `util/format/` handles parsing, hashes, and short text

## Folders

- `core/`: shared result and import plan types. [core.md](core.md)
- `model/`: level state, parser, header merge. [model.md](model.md)
- `diff/`: changes between two saved versions. [diff.md](diff.md)
- `identity/`: stable object IDs across saves. [identity.md](identity.md)
- `store/`: SQLite database and `.gdge` files. [store.md](store.md)
- `service/`: commit, checkout, revert, merge, plus `GdgeExport` (write `.gdge`) and `GdgeImportMerge` (multi-file import). [service.md](service.md)
- `editor/`: level key and editor read/write. [editor.md](editor.md)
- `hooks/`: pause menu entry. [hooks.md](hooks.md)
- `ui/`: popups and labels. [ui.md](ui.md)
- `settings/`: mod settings handlers. [settings.md](settings.md)
- `util/`: worker, zip, blobs, parsing. [util.md](util.md)
- `test/`: in-mod automated suites. [test.md](test.md)
- `sqlite/`: built-in SQLite library. [sqlite.md](sqlite.md)

## See also

- [docs/index.md](../index.md): all docs
