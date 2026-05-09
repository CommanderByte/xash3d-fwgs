# Library Locator TODO

## Purpose

Track migration of `FS_FindLibrary` and related DLL/shared-library lookup
policy from `filesystem.c`.

## Migration Order

- [x] Expand tests for direct-path library lookup and relative path quirks.
- [x] Add target-neutral library short-path normalization helper.
- [ ] Route encrypted library detection through a small helper.
- [ ] Route `FS_FindLibrary` through `LibraryLocator`.

## Boundaries

- Always reset direct-path mode after temporary library lookup.
- Preserve platform library extension and prefix behavior.
- Preserve archived-library rejection/metadata behavior.
