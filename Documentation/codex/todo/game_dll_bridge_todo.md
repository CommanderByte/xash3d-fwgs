# Game DLL Bridge TODO

This TODO expands Phase 86 into a bounded implementation lane. The bridge is
high risk because it is the compatibility boundary used by external game DLLs.
Move only narrow, tested behavior behind adapters until the loader, edicts,
string base, and callback table publication are ready for an explicit ABI
phase.

## Scope Start

- Start with callback inventory, table metadata, user-message state, and small
  target-neutral policy helpers.
- Prefer tests that do not require loading a real game DLL.
- Keep `enginefuncs_t`, `DLL_FUNCTIONS`, `NEW_DLL_FUNCTIONS`, `edict_t`,
  `globalvars_t`, `entvars_t`, `string_t`, and `SAVERESTOREDATA` ABI-stable.
- Keep live `sv`, `svs`, `svgame`, message buffers, cvars, commands,
  filesystem probes, and DLL lifetime in adapters until each dependency has a
  modern owner.

## Scope End

- End the first bridge lane before replacing DLL load/unload, edict array
  allocation, `globalvars_t::pStringBase`, save/restore runtime callback
  ordering, movement, trace, and visibility behavior.
- Do not introduce a new public C++ plugin API in this lane.
- Do not throw exceptions across game DLL callbacks.

## Phase 87: Enginefuncs Metadata

- [x] Build a table-slot inventory for `enginefuncs_t`.
  Evidence: `src/include/engine/server/game_dll_enginefuncs.hpp`.
- [x] Categorize every callback by subsystem, adapter owner, and migration
  readiness.
  Evidence: `src/include/engine/server/game_dll_enginefuncs.hpp`.
- [x] Add tests or compile-time checks that table metadata remains complete.
  Evidence: `tests/engine/game_dll_enginefuncs.cpp`.
- [x] Keep the concrete `gEngfuncs` table and `engine/eiface.h` ABI unchanged.
  Evidence: Phase 87 touched only modern metadata/test/build/doc files.

## Phase 88: Message Session Facade

- [x] Baseline `pfnMessageBegin()`, `pfnMessageEnd()`, write primitives, and
  rewrite rules in enough detail for golden tests.
  Evidence: `sv_game.c` currently keeps one active message at a time, clamps
  message numbers to `svc_bad..255`, treats `svc_temp_entity` and variable
  user messages as size-prefixed payloads, clears `sv.multicast` on overflow
  or size mismatch, maps `pfnWriteByte(-1)` to `0xff`, counts null strings as
  one byte, appends an empty string to empty `svc_finale`/`svc_cutscene`, and
  can rewrite GoldSrc `svc_spawnstaticsound` to `svc_sound` when the bug
  compatibility flag is enabled.
- [x] Implement a target-neutral message-session state machine that writes to
  mock buffers.
  Evidence: `src/include/engine/server/game_dll_message_session.hpp` and
  `src/engine/server/game_dll_message_session.cpp`.
- [x] Add tests for double begin, end without begin, fixed-size mismatch,
  variable-size patching, overflow clearing, `pfnWriteByte(-1)`, string null
  accounting, and rewrite admission.
  Evidence: `tests/engine/game_dll_message_session.cpp`.
- [x] Route only the smallest safe validation and size-accounting decisions.
  Evidence: `engine/server/game_dll_message_session_adapter.cpp` routes byte
  normalization, fixed write byte counts, string byte counts, entity index
  validation, destination clamping, and rewrite admission while leaving
  `sv.multicast` writes and `SV_Multicast()` in `sv_game.c`.

## Phase 89: User Message Registry Policy

- [x] Baseline `pfnRegUserMsg()` duplicate, invalid-name, invalid-size, and
  active-server resend behavior.
  Evidence: `pfnRegUserMsg()` returns `svc_bad` for null/empty names, names
  that do not fit the 32-byte legacy slot, sizes above `MAX_USERMSG_LENGTH`,
  and a full table. Sizes below `-1` clamp to `-1`; duplicate names return the
  existing message number; new messages use slot `i`, number `svc_lastmsg + i`,
  and trigger `SV_SendUserReg()` plus `MSG_ALL` multicast only while the server
  is active.
- [x] Implement a target-neutral registry policy that can be tested without
  live `svgame.msg` mutation.
  Evidence: `src/include/engine/server/game_dll_user_message_registry.hpp`
  and `src/engine/server/game_dll_user_message_registry.cpp`.
- [x] Add tests for duplicate names, fixed/variable sizes, max-name length,
  max-message count, and active resend planning.
  Evidence: `tests/engine/game_dll_user_message_registry.cpp`.
- [x] Keep actual message IDs and multicast writes adapter-owned.
  Evidence: `engine/server/game_dll_user_message_registry_adapter.cpp`
  returns a policy plan; `sv_game.c` still mutates `svgame.msg`, calls
  `SV_SendUserReg()`, and calls `SV_Multicast()`.

## Phase 90: Game DLL Text, Command, And Alert Output Policy

- [x] Baseline `pfnServerCommand()`, `pfnClientCommand()`,
  `pfnClientPrintf()`, `pfnServerPrint()`, `pfnAlertMessage()`, and
  `pfnEndSection()`.
  Evidence: `pfnServerCommand()` queues only valid commands. `pfnClientCommand()`
  skips inactive servers and fake clients, reports missing clients, and writes
  valid stufftext only. `pfnClientPrintf()` rejects non-clients, skips fake
  clients, maps console/chat to `SV_ClientPrintf()`, and maps center text to
  `svc_centerprint`. `pfnServerPrint()` broadcasts in Quake-compatible mode
  and otherwise prints to console. `pfnAlertMessage()` logs multiplayer
  `at_logged`, suppresses output at developer `0`, gates `at_aiconsole` on
  extended developer level, and maps remaining alert classes to existing
  console/log sinks. `pfnEndSection()` opens credits only for
  `oem_end_credits`, otherwise queues disconnect.
- [x] Extract command validation and output-classification decisions where
  they do not depend on live sinks.
  Evidence: `src/include/engine/server/game_dll_output_policy.hpp` and
  `src/engine/server/game_dll_output_policy.cpp`.
- [x] Add tests for fake-client skips, invalid commands, developer verbosity,
  multiplayer `at_logged`, and aiconsole suppression.
  Evidence: `tests/engine/game_dll_output_policy.cpp`.
- [x] Keep `Cbuf_AddText()`, command execution, print sinks, and log files
  legacy-owned.
  Evidence: `engine/server/game_dll_output_policy_adapter.cpp` returns routing
  decisions; `sv_game.c` still calls `Cbuf_AddText()`, `SV_ClientPrintf()`,
  `Con_Printf()`, `Con_DPrintf()`, `Log_Printf()`, `Host_Credits()`, and
  `SV_WriteClientStuffTextMessage()`.

## Phase 91: Resource And Precache Callback Policy

- [x] Baseline model, sound, generic, decal, and event precache callback
  behavior.
- [x] Extract optional-resource admission, slash normalization, case-insensitive
  lookup, and error-plan decisions.
- [x] Add tests for null/empty names, leading `!`, leading slashes, duplicate
  lookup, missing optional resources, and bounds failures.
- [x] Keep actual resource tables, model loads, filesystem probes, and fatal
  errors adapter-owned.

## Phase 92: Game DLL Sound, Decal, And Static Payload Bridge

- [x] Reuse completed server sound/static/decal helpers where possible.
- [x] Baseline the game-DLL-facing callback inputs and legacy validation
  behavior.
- [x] Add tests for ambient sound, static decal, static entity, particle, and
  lightstyle callback plans.
- [x] Keep multicast, signon, and resource-index ownership in legacy adapters.

## Phase 93: Client Info-Key And Query Callback Policy

- [x] Baseline `pfnGetInfoKeyBuffer()`, `pfnSetValueForKey()`,
  `pfnSetClientKeyValue()`, physics info callbacks, auth/user ID callbacks,
  and cvar query callbacks.
- [x] Extract safe admission and fallback-result policies.
- [x] Add tests for local/serverinfo selection, unchanged key-values,
  resend-flag decisions, bad player query results, and game-dir compatibility.
- [x] Keep `Info_*` mutation and live client fields adapter-owned.

## Phase 94: String Pool Compatibility Fixtures

- [x] Baseline `SV_ProcessString()`, `SV_AllocStringPool()`,
  `SV_AllocString()`, `SV_MakeString()`, `SV_GetString()`, and string stats.
  Evidence: `Documentation/codex/modern/engine/game-dll-string-pool-compatibility.md`.
- [x] Add fixtures for empty strings, escape normalization, dedup behavior,
  duplicate-disabled behavior, invalid handles, and overflow reset.
  Evidence: `tests/engine/game_dll_string_pool_compat.cpp`.
- [x] Keep `globalvars_t::pStringBase`, 64-bit near-DLL storage, and physics
  string overrides legacy-owned until fixtures are broad enough.
  Evidence: Phase 94 adds only `src/engine/server/game_dll_string_pool_compat.cpp`;
  live `SV_AllocString()`, `SV_MakeString()`, and `SV_GetString()` remain in
  `engine/server/sv_game.c`.

## Phase 95: Entity Handle And Private Data Policy

- [ ] Baseline edict index/pointer helpers, private-data allocation, free
  ordering, and `BUGCOMP_PENTITYOFENTINDEX_FLAG`.
- [ ] Add tests around pure index admission, all-entity versus client-visible
  lookup decisions, private-data size rounding, and destructor ordering plans.
- [ ] Keep actual `edict_t` memory, `pvPrivateData`, and game DLL destructor
  calls in the adapter until a loaded-DLL fixture exists.

## Phase 96: Entity Parse And Spawn Boundary

- [ ] Baseline `SV_ParseEdict()`, `SV_LoadFromFile()`, classname ordering,
  utility-key discard, angle-to-angles rewrite, and custom entity handling.
- [ ] Add parser/plan tests that do not invoke real game entity code.
- [ ] Keep `pfnKeyValue()`, `pfnSpawn()`, edict allocation, and map text
  lifetime legacy-owned.

## Phase 97: Changelevel And Save/Restore Bridge Policy

- [ ] Baseline `pfnChangeLevel()`, `SV_QueueChangeLevel()`,
  `SV_WriteEntityPatch()`, and game callback sequencing in save/restore.
- [ ] Add tests for duplicate changelevel suppression, landmark truncation,
  invalid level names, and save patch planning.
- [ ] Keep runtime save/load streams and game DLL field serialization
  legacy-owned.

## Phase 98: Visibility And Trace Callback Boundary

- [ ] Baseline trace and visibility callback wrappers after world/trace
  fixtures are available.
- [ ] Add pure tests for admission and result conversion only.
- [ ] Keep actual hull, BSP, leaf, PVS/PAS, and collision work in legacy code
  until a broader world migration phase.

## Phase 99: Movement And Fake-Client Callback Boundary

- [ ] Baseline yaw, pitch, move-to-origin, walkmove, set-origin, maxspeed, and
  fake-client `pfnRunPlayerMove()` behavior.
- [ ] Add tests for pure movement-policy values only after movement fixtures
  exist.
- [ ] Keep `SV_RunCmd()`, `playermove_t`, `sv.current_client`, and physics
  callbacks legacy-owned.

## Phase 100: DLL Load/Unload Facade Plan

- [ ] Baseline missing-export, version mismatch, fallback API, physics API,
  command/cvar unlink, string pool, and memory-pool cleanup paths.
- [ ] Implement a pure load-plan helper against fake symbol tables only.
- [ ] Keep `COM_LoadLibrary()`, `COM_UnloadLibrary()`, real symbol lookup,
  `GiveFnptrsToDll()`, edict allocation, and callback table publication in
  legacy code until the final bridge phase.

## Phase 101: Game DLL Bridge Manual Validation

- [ ] After each routed bridge slice, run focused tests, full tests,
  `scripts/run-phase-validation.ps1`, and record `+wait +wait` first-frame
  time in `Documentation/codex/tasks.md`.
- [ ] Every few routed slices, run `scripts/run-game.ps1` and manually start a
  new game.
- [ ] Record any mod-specific or Half-Life asset-loading failures as bridge
  compatibility notes before continuing.
