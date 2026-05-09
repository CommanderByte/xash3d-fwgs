# Backend Implementation Migration TODO

## Purpose

Track the larger migration from legacy implementation bodies in `filesystem/`
to modern implementation bodies in `src/filesystem`.

This TODO begins after the adapter pilot phases. The previous work created
modern scaffolds, C adapters, and behavior tests. The next work should move the
actual parsing, lookup, search, open, and load behavior behind those modern
objects.

## Migration Order

- [x] Classify every `filesystem/` file as facade, adapter, legacy
  implementation, or build/public glue.
- [x] Migrate WAD implementation body into `src/filesystem/wad_backend.cpp`.
- [ ] Migrate PAK implementation body into `src/filesystem/pak_backend.cpp`.
- [ ] Migrate ZIP/PK3 implementation body into
  `src/filesystem/zip_backend.cpp`.
- [ ] Migrate directory cache/search/case-fix behavior into
  `src/filesystem/directory_backend.cpp`.
- [ ] Migrate Android asset implementation into
  `src/filesystem/android_assets_backend.cpp` while preserving desktop
  compileability.
- [ ] Introduce `FilesystemRuntime` as the owner of state, search paths,
  gameinfo-derived mounts, policies, and diagnostics.
- [ ] Route `VFileSystem009.cpp` through the modern runtime while preserving
  the public vtable and `CreateInterface` behavior.
- [ ] Update build scripts so `src/filesystem` is the implementation source
  tree and `filesystem/` only provides facades/adapters.
- [ ] Move completed TODO and audit documents into `Documentation/codex/done/`
  after the evidence is committed and reviewed.
  Note: the first completed TODO archival pass moved fully complete checklist
  files into `Documentation/codex/done/todo/`; keep this item open for future
  implementation-body migration docs and audit notes.

## Testing Rule

Each migration phase must include at least one of:

- a new focused unit test that freezes behavior before moving code,
- an expanded existing fixture that covers a newly identified legacy quirk,
- a wrapper-facing test if public ABI behavior is touched.

The full `.\waf.bat build` and Windows runtime smoke test should run after any
phase that changes mounted paths, archive parsing, file handles, search result
assembly, library lookup, or public facades.

## Boundaries

- Do not move multiple archive backend bodies in the same commit.
- Do not expose `src/filesystem` classes through `filesystem.h` or
  `VFileSystem009.h`.
- Do not remove a legacy source file until its replacement is wired into the
  build and all existing tests pass.
- Do not move TODO/audit documents to `done/` until their implementation is
  verifiably complete.
