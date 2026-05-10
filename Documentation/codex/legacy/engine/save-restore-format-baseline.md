# Save/Restore Format Baseline

Phase: 83

Legacy owner: `engine/server/sv_save.c`

## Scope

This baseline covers the binary containers and compatibility quirks around the
server save/restore path. It does not attempt to reimplement game DLL field
serialization, entity creation, global-state restoration, or renderer/audio
state restoration.

## Magic And Versions

There are two magic values:

- `SAVEFILE_HEADER`: numeric `0x564C4156`, written little-endian as `VALV`.
- `SAVEGAME_HEADER`: numeric `0x5641534A`, written little-endian as `JSAV`.

Versions:

- Main save version: `0x0071`.
- Client-side per-level version: `0x0067`.
- `SV_GetSaveComment()` has special user-facing text for `0x0065`, older
  versions, and newer versions.

## Per-Level Server File: `save/<map>.HL1`

The server entity file uses `VALV` and version `0x0071`.

Disk layout:

1. `int id`.
2. `int version`.
3. `int size`: total data bytes used to initialize the save buffer. This
   includes entity-table field sections and normal save data.
4. `int tableCount`: number of entity table records.
5. `int tokenCount`: token-table slots.
6. `int tokenSize`: byte length of the linear token table.
7. `tokenSize` bytes of NUL-terminated token strings.
8. Serialized entity table field sections.
9. Serialized save header, adjacency, lightstyle, and entity data field
   sections.

Important ordering quirk: save writes the token table, then the entity table,
then normal save data. Restore reads tokens first, then parses `tableCount`
`ETABLE` sections before rebasing `pBaseData` to the remaining save payload.

## Per-Level Client File: `save/<map>.HL2`

The client file uses `JSAV` and version `0x0067`.

Disk layout:

1. `int id`.
2. `int version`.
3. `int size`: client save data bytes, excluding token table.
4. `int tokenCount`.
5. `int tokenSize`.
6. `tokenSize` bytes of tokens.
7. Serialized `ClientHeader`.
8. Decal, static-entity, and sound sections.

The code stores `viewentity` as a character array of `sizeof(short)` instead of
`FIELD_SHORT` because some HLU SDK based mods reject short fields.

Phase 84 adds an explicit modern fixture guard for this assumption:
`sizeof(short)` must be two bytes, and the fixture parser treats the stored
bytes as the legacy packed 16-bit `viewentity` value. Runtime loading still
uses the original `SAVE_CLIENT` structure and game DLL field callbacks.

## Entity Patch File: `save/<map>.HL3`

`HL3` is a small removed-entity patch file.

Disk layout:

1. `int count`.
2. `count` integers containing entity table indexes marked as
   `FENTTABLE_REMOVED`.

The reader does not perform explicit bounds checking before marking table
entries, so generated compatibility fixtures should include malformed index
coverage before this code is modernized.

Phase 84 adds read-only parser coverage that reports negative and out-of-range
indexes as malformed instead of applying them. This documents the safe target
behavior for a later runtime migration; the legacy `EntityPatchRead()` path is
not routed through the modern parser yet.

## Outer Save File: `save/<slot>.sav`

The visible save slot uses `JSAV` and version `0x0071`.

Disk layout:

1. `int id`.
2. `int version`.
3. `int size`: game header/global data size, excluding token table.
4. `int tokenCount`.
5. `int tokenSize`.
6. `tokenSize` bytes of tokens.
7. Serialized `GameHeader`.
8. Serialized game global state.
9. Bundled `HL?` files copied from `save/*.HL?`.

Each bundled file entry is:

1. `MAX_OSPATH` bytes of NUL-padded file name.
2. `int fileSize`.
3. `fileSize` bytes of file content.

The number of bundled files comes from `GAME_HEADER.mapCount`.

## Token Table Quirk

`StoreHashTable()` writes exactly `tokenCount` strings, each NUL-terminated.
Empty token slots are written as a single NUL byte.

`BuildHashTable()` reads `tokenSize` bytes from disk, then walks exactly
`tokenCount` NUL-terminated strings and rebases `pBaseData` to the pointer after
the strings it consumed. That rebased in-memory pointer can differ from the
disk payload offset if `tokenSize` contains trailing bytes after the expected
strings. A modern parser should track both values.

## Field Section Shape

The save field stream is produced by game DLL save/restore callbacks. The
engine consumes it as a sequence of sections:

1. `short sectionHeaderSize`.
2. `short sectionNameTokenIndex`.
3. `sectionHeaderSize` bytes of section header data. The first byte is treated
   as field count by the existing `SV_GetSaveComment()` fast path.
4. For each field:
   - `short fieldSize`.
   - `short fieldNameTokenIndex`.
   - `fieldSize` bytes of field data.

Known section names written by `sv_save.c` include `GameHeader`, `Save Header`,
`ADJACENCY`, `LIGHTSTYLE`, `ETABLE`, `ClientHeader`, `DECALLIST`,
`STATICENTITY`, and `SOUNDLIST`.

`LIGHTSTYLE` serialization is non-empty only. `SaveGameSlot()` first counts
lightstyles whose pattern begins with a non-NUL byte, writes that value as
`SAVE_HEADER.lightStyleCount`, and then emits exactly those non-empty
`LIGHTSTYLE` sections. Empty style slots are neither counted nor written.

## Compatibility Notes

- The strict on-disk headers should stay byte-compatible.
- Field serialization remains game-DLL-owned for now.
- Save header time is written with `pSaveData->time = 0.0f` to preserve old-save
  compatibility before restoring the real header time.
- Landmark offsets are applied to decals and cross-level entities during
  transition restore.
- Permanent decals do not move across adjacent-level restores.
- Save/load validity is tied to single-player, active local client state,
  non-background maps, non-intermission, alive player, and optional game DLL
  veto callbacks.
