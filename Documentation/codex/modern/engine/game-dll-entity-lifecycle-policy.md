# Game DLL Entity Lifecycle Policy

Phase 95 extracts small entity handle and private-data decisions from
`sv_game.c` while keeping live entity memory and game DLL destructor ownership
in the legacy server.

## Legacy Baseline

- `SV_PEntityOfEntIndex()` accepts only indices in `[0, max_edicts)`.
- Index `0` always returns the world edict when it is in range.
- `ENGINE_QUAKE_COMPATIBLE` returns the edict pointer for any in-range index,
  without requiring private data.
- Otherwise, valid edicts with `pvPrivateData` are returned.
- World and player slots can be returned without private data. The normal
  all-entity callback treats player slots as `index <= maxclients`.
- The `BUGCOMP_PENTITYOFENTINDEX_FLAG` swaps `pfnPEntityOfEntIndex` to the
  GoldSrc-compatible broken path, which treats player slots as
  `index < maxclients`; that makes the last client fail the player-slot
  fallback unless it has private data.
- `pfnIndexOfEdict()` returns `0` for null edicts, computes the pointer offset
  from `svgame.edicts`, and fatals only when the computed number is below `0`
  or above `max_edicts`. A one-past `max_edicts` result is therefore not fatal.
- `pfnPEntityOfEntOffset()` and `pfnEntOffsetOfPEntity()` are raw pointer
  arithmetic helpers and do not validate offsets.
- `pfnPvAllocEntPrivateData()` asserts the edict, frees existing private data,
  and allocates `(cb + 15) & ~15` bytes only when `cb > 0`.
- `SV_FreePrivateData()` skips null edicts and edicts without private data. If
  `NEW_DLL_FUNCTIONS::pfnOnFreeEntPrivateData` exists, it runs before the
  engine checks whether the current pointer is owned by `svgame.mempool`.
  The pointer is cleared after the optional destructor and optional free.
- `pfnPvEntPrivateData()` returns `pEdict->pvPrivateData` for non-null edicts
  and null otherwise.

## Modern Boundary

`src/engine/server/game_dll_entity_lifecycle.cpp` owns target-neutral plans for:

- entity-index lookup admission and the player-slot bugcompat distinction;
- null, valid, one-past, and fatal edict-index decisions;
- private-data allocation size rounding;
- private-data free ordering decisions.

`engine/server/game_dll_entity_lifecycle_adapter.cpp` routes only those
decisions back into `sv_game.c`.

The legacy server still owns:

- `svgame.edicts`, `edict_t`, `entvars_t`, and `pvPrivateData` storage;
- `SV_EdictNum()`, `SV_IsValidEdict()`, and pointer arithmetic against the
  actual edict array;
- `Mem_Calloc()`, `Mem_Free()`, and `Mem_IsAllocatedExt()`;
- `NEW_DLL_FUNCTIONS::pfnOnFreeEntPrivateData`;
- `SV_FreeEdict()`, `SV_InitEdict()`, and full entity reuse/lifetime rules.

This keeps the game DLL ABI stable while making the dangerous edge decisions
unit-testable before any broader edict ownership migration.
