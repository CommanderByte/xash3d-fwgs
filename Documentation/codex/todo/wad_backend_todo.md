# WAD Backend TODO

## Purpose

This TODO tracks the eventual WAD backend migration. WAD should follow PAK
because WAD lump lookup has more compatibility-specific behavior.

## Current Legacy Responsibilities

- Load WAD2/WAD3 headers and lump tables.
- Reject bad, empty, oversized, or corrupted WAD files.
- Map lump names and type/extension behavior for lookup and load.
- Support WADs packed inside other archives through `FS_LOAD_PACKED_WAD`.
- Implement find, search, file time, print info, close, open, and lump load.

## Migration Order

- [ ] Expand WAD fixture tests before implementation movement.
- [ ] Add a `WadBackend` skeleton after PAK proves the bridge pattern.
- [ ] Preserve packed-WAD paths such as `pak0.pak/inside.wad`.
- [ ] Forward callbacks one at a time through a C adapter.
- [ ] Keep `FS_AddWad_Fullpath` callable from C.

## Boundaries

- Do not change lump extension/type compatibility during the first bridge.
- Do not merge WAD behavior into generic archive behavior prematurely.
