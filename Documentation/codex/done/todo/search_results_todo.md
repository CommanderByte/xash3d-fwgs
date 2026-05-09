# Search Results TODO

## Purpose

Track migration of `FS_Search` and `search_t` allocation/result assembly from
`filesystem.c`.

## Migration Order

- [x] Add focused tests for search ordering and duplicate filtering.
- [x] Add target-neutral `SearchResultBuilder` over temporary string lists.
- [x] Preserve `search_t` packed allocation layout.
- [x] Route `FS_Search` result assembly through the builder.
- [x] Keep `VFileSystem009` search handle behavior covered.

Evidence: `filesystem/search_result_builder_adapter.cpp` owns duplicate
compaction, packed allocation byte calculation, and `search_t` initialization;
`.\waf.bat build --targets=test_search-results,test_filesystem_search_result_builder`,
direct `build\filesystem\test_search-results.exe`, direct
`build\src\test_filesystem_search_result_builder.exe`, and
`build\filesystem\test_interface.exe` passed on 2026-05-09.

## Boundaries

- Do not change `search_t` ownership; callers still free through `FS_FreeSearch`.
- Preserve case-insensitive matching behavior per backend.
- Preserve gamedir-only filtering.
