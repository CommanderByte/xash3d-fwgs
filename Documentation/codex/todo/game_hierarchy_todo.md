# Game Hierarchy TODO

## Purpose

Track extraction of game directory and mount-order construction from
`filesystem.c`.

## Migration Order

- [x] Add `GameHierarchyBuilder` design note with current mount ordering.
- [ ] Add fixtures for any hierarchy behavior not covered by
  `tests/filesystem/hierarchy.c`.
- [x] Extract a target-neutral mount request record type.
- [x] Build mount requests separately from applying them to `searchpath_t`.
- [x] Route `FS_LoadGameInfo` hierarchy mounting through the builder.

## Boundaries

- Do not change falldir, rodir, HD, LV, addon, localization, custom, or
  downloads ordering.
- Do not move gameinfo parsing in the same pass as mount-order construction.
- Keep archive mounting delegated to existing archive factories.
