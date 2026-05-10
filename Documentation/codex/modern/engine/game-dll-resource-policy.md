# Game DLL Resource Policy

Phase 91 extracts the target-neutral decisions behind game-DLL-facing resource
and precache callbacks without moving ownership of live server resource state.

## Legacy Baseline

- `pfnPrecacheModel()` accepts a leading `!` to mark a model optional, strips
  that prefix before indexing, and only sets `RES_FATALIFMISSING` for required
  models.
- Model lookup and model registration strip one leading slash or backslash,
  copy into a `MAX_QPATH` buffer, normalize slash direction, collapse duplicate
  slashes, and compare precache names case-insensitively.
- Sound registration rejects leading `!` sentence names with a warning instead
  of precaching them.
- Generic and event registration normalize slashes but do not strip leading
  slashes.
- Decal lookup rejects empty names and uses case-insensitive lookup, but does
  not normalize decal strings.
- Table overflow remains fatal through the legacy adapter because it owns the
  table capacity, resource arrays, and `Host_Error()` call site.

## Modern Boundary

`src/engine/server/game_dll_resource_policy.cpp` now owns only pure decisions:

- resource-name admission and normalization;
- optional model prefix handling;
- sound sentence-name rejection;
- case-insensitive duplicate matching;
- table-slot overflow classification;
- required versus optional model load classification.

`engine/server/game_dll_resource_policy_adapter.cpp` exposes those decisions to
the legacy server. The legacy side still owns:

- `sv.model_precache`, `sv.sound_precache`, `sv.files_precache`, and
  `sv.event_precache`;
- `host.draw_decals`;
- `SV_SendSingleResource()`;
- `Mod_ForName()`;
- warning, console, and fatal error output.

This keeps the game DLL ABI stable while making the policy testable before a
larger resource catalog migration.
