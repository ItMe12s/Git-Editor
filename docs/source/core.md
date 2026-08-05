# core

## Summary

Shared types used by the main logic and import steps. UI sees messages and counts, not these internal types.

## Files

- `Result.hpp`: `Result<T>` alias for `std::expected<T, std::string>`
- `ImportPlan.hpp`: `ImportPlan` plus `RevertPayload`, `ImportManyPayload`, and `InvalidImport`

## Notes

`GitService` and `GdgeImportPlanner` use these types.

## Related

- [README.md](README.md)
- [service.md](service.md)
- [store.md](store.md)

## Source

- `src/core/Result.hpp`
- `src/core/ImportPlan.hpp`
