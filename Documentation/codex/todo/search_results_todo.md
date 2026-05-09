# Search Results TODO

## Purpose

Track migration of `FS_Search` and `search_t` allocation/result assembly from
`filesystem.c`.

## Migration Order

- [ ] Add focused tests for search ordering and duplicate filtering.
- [ ] Add target-neutral `SearchResultBuilder` over temporary string lists.
- [ ] Preserve `search_t` packed allocation layout.
- [ ] Route `FS_Search` result assembly through the builder.
- [ ] Keep `VFileSystem009` search handle behavior covered.

## Boundaries

- Do not change `search_t` ownership; callers still free through `FS_FreeSearch`.
- Preserve case-insensitive matching behavior per backend.
- Preserve gamedir-only filtering.
