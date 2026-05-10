# Server Static Entity And Decal Baseline

Phase 77 covers the small, stable parts of the server static-entity and decal
message paths. The safe extraction boundary is asymmetric:

- `svc_bspdecal` has a compact fixed payload and can be moved to a modern
  payload writer.
- `svc_spawnstatic` uses `MSG_WriteDeltaEntity()` with active delta tables and
  `entity_state_t` fields. Phase 77 only models the safe admission checks and
  command identity; the delta payload stays in legacy code.

## Decal Writer

`SV_CreateDecal()` in `engine/server/sv_game.c` writes static map decals and
demo-restored decals.

Legacy behavior:

- if writing to `sv.signon`, static decals are accepted only while
  `sv.state == ss_loading`;
- when fewer than 20 bytes remain, `sv.ignored_world_decals` is incremented and
  nothing is written;
- `MSG_BeginServerCmd(msg, svc_bspdecal)` writes the command byte;
- the payload is:
  - origin XYZ with `MSG_WriteVec3Coord()`;
  - decal index as `MSG_WriteWord()`;
  - entity index as `MSG_WriteShort()`;
  - model index as `MSG_WriteWord()` only when `entityIndex > 0`;
  - flags as `MSG_WriteByte()`;
  - scale encoded as `MSG_WriteWord(scale * 4096)`.

`CL_ParseStaticDecal()` mirrors this shape. It reads the model index only for
positive entity indexes, then converts scale back with `/ 4096.0f`.

Coordinate encoding follows the shared legacy coordinate writer:

- default mode stores `(int)(coord * 8.0f)` as a signed 16-bit value;
- `ENGINE_WRITE_LARGE_COORD` stores `Q_rint(coord)` as a signed 16-bit value.

## Static Entity Writer

`SV_CreateStaticEntity()` writes `svc_spawnstatic` for entities produced by
`pfnMakeStatic()` and for save/restore static entity rehydration.

Legacy behavior:

- indexes at or above `MAX_STATIC_ENTITIES - 1` are rejected;
- the first overflow logs a warning and sets `sv.static_ents_overflow`;
- rejected static entities increment `sv.ignored_static_ents`;
- messages with fewer than 50 bytes left are rejected and counted as ignored;
- the static entity state is stored in `svs.static_entities[index]`;
- `state->modelindex` is restored from the model name stored in
  `state->messagenum`;
- `state->entityType` is forced to `ENTITY_NORMAL`;
- `state->number` is forced to `0`;
- `SV_FindBestBaseline()` picks the delta baseline;
- `MSG_BeginServerCmd(msg, svc_spawnstatic)` writes the command byte;
- `MSG_WriteDeltaEntity()` writes the actual static entity delta payload.

The client reads `svc_spawnstatic` by first reading an entity number with
`MAX_ENTITY_BITS`, then calling `MSG_ReadDeltaEntity()` with `DELTA_STATIC`.
Because that payload depends on delta table initialization and entity-state
encoding, Phase 77 leaves it legacy-owned.

## Restart Paths

`SV_RestartStaticEnts()` clears client-side static entities and resends all
server static entities into `sv.reliable_datagram` using
`SV_CreateStaticEntity()`.

`SV_RestartDecals()` is used for demo recording/client restart behavior. It
asks the renderer for current decals, clears renderer decals, lets the game DLL
restore custom decals if possible, resolves decal names, and then calls
`SV_CreateDecal()` for non-studio decals.

Save/restore in `sv_save.c` also calls these writers while rebuilding signon
data. That path keeps entity lookup, landmark handling, trace decisions, and
game-DLL custom restore callbacks outside the modern payload helper.

## Compatibility Notes

- Decal index, entity index, model index, flags, and scale are not validated in
  the payload helper. Legacy callers remain responsible for validity.
- `svc_spawnstatic` delta payload extraction is deferred until the delta encoder
  itself has a modern boundary.
- Signon/reliable buffer ownership remains in legacy code.
- The modern helper mirrors `MSG_WriteVec3Coord()` and `MSG_WriteWord()` quirks
  rather than introducing new clamping.
