# core

Shared types used by the main logic and import steps.

## Main files

- `Result.hpp`: success or error wrapper (`value` / `error`)
- `ImportPlan.hpp`: groups of import files and the data they carry

## Touches

`GitService` and `GdgeImportPlanner` use these types. UI sees messages and counts, not the internal types.

## You might care if

Contributors only.

## Code

- [src/core/Result.hpp](../../src/core/Result.hpp)
- [src/core/ImportPlan.hpp](../../src/core/ImportPlan.hpp)
