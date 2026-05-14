# Game DLL Bridge Submodule Plan

Phase 121 turns the completed Phase 87-100 game DLL bridge lane into a
submodule plan. The goal is not to move files immediately. The goal is to know
which modern helpers can eventually live together under a coherent
`game_dll/` ownership tree while the public game DLL ABI remains unchanged.

## Current Surface

`sv_game.c` is still the compatibility shell for the game DLL bridge. It owns
the concrete `enginefuncs_t` table, the exported callback functions, live
`svgame` state, and the C calling conventions expected by external game DLLs.

The modern helper surface is already broad:

| Bridge area | Modern helpers | Current legacy adapter or owner |
| --- | --- | --- |
| ABI metadata and load planning | `game_dll_enginefuncs`, `game_dll_load_policy` | `sv_game.c` still owns `gEngfuncs`, `SV_LoadProgs()`, `SV_UnloadProgs()`, `COM_LoadLibrary()`, `GiveFnptrsToDll()`, and API fallback calls. |
| Messaging | `game_dll_message_session`, `game_dll_user_message_registry`, `game_dll_payload_policy`, `server_message_envelope` | `sv_game.c` still owns `pfnMessageBegin()`, `pfnMessageEnd()`, write callbacks, user-message table mutation, `sv.multicast`, and `SV_Multicast()`. |
| Resources and precache callbacks | `game_dll_resource_policy`, `server_resource_catalog`, `resource_identity` | `sv_game.c`, `sv_init.c`, and resource arrays still own actual model/sound/generic/event/decal indexes and filesystem probes. |
| Entity lifecycle and parsing | `game_dll_entity_lifecycle`, `game_dll_entity_parse` | `sv_game.c` still owns edict allocation, private-data storage, destructor calls, map entity parse ordering, `pfnKeyValue()`, and `pfnSpawn()`. |
| Client info and query callbacks | `game_dll_client_info_policy` | `sv_game.c` and `sv_client.c` still own live client arrays, info-string mutation, cvar query messages, and game DLL callback dispatch. |
| Output and commands | `game_dll_output_policy` | `sv_game.c` still calls `Cbuf_*`, `SV_ClientPrintf()`, console/log sinks, credits, and disconnect logic. |
| String pool compatibility | `game_dll_string_pool_compat` | `sv_game.c` still owns `globalvars_t::pStringBase`, near-DLL storage, physics string overrides, and live string handles. |
| Changelevel and save intent | `game_dll_changelevel_policy` | `sv_game.c` and `sv_save.c` still own runtime save/load streams, entity patches, token tables, and game DLL serializer callbacks. |
| Visibility and trace callbacks | `game_dll_visibility_trace_policy` | `sv_game.c` and `sv_world.c` still own BSP, hulls, traces, PVS/PAS, leaf mutation, and collision results. |
| Movement callbacks | `game_dll_movement_policy`, `server_movement_constraints` | `sv_game.c`, `sv_move.c`, `sv_pmove.c`, and `sv_phys.c` still own `SV_RunCmd()`, PMove state, physics callbacks, traces, and entity relinking. |

## Future `game_dll/` Tree

When the project is ready to regroup files physically, use this shape under
`src/engine/server/game_dll/` and mirror it under
`src/include/engine/server/game_dll/`:

```text
game_dll/
  bridge/
  messaging/
  resources/
  entities/
  client_info/
  output/
  string_pool/
  changelevel/
  world_queries/
  movement/
```

Recommended ownership:

| Submodule | Can move there later | Must stay outside for now |
| --- | --- | --- |
| `bridge/` | Callback table metadata, fake-symbol load planning, optional API planning, compatibility classification. | Concrete `gEngfuncs` publication, `COM_LoadLibrary()`, `COM_UnloadLibrary()`, `COM_GetProcAddress()`, `GiveFnptrsToDll()`, and real game DLL lifetime. |
| `messaging/` | Message-session state, user-message registration policy, write-size accounting, rewrite admission, game-DLL-facing payload plans. | `sv.multicast`, destination buffers, `SV_Multicast()`, netchan sends, and active `svgame.msg` mutation until aggregate tests cover them. |
| `resources/` | Game-DLL-facing resource/precache admission, optional-resource handling, slash normalization, lookup plans, event/decal index policy. | `sv.resources[]`, model loads, sound/generic/event arrays, filesystem probes, HPAK, and fatal error effects. |
| `entities/` | Entity index admission, private-data allocation/free plans, classname/keyvalue parse plans, custom-entity fallback metadata. | Edict array allocation, `pvPrivateData`, live constructor/destructor calls, `pfnKeyValue()`, `pfnSpawn()`, and runtime map text ownership. |
| `client_info/` | Info-key selection, key mutation admission, cvar-query fallback plans, auth/user-ID lookup plans, game-dir compatibility. | Live client array mutation, `Info_*` side effects, cvar query packet writes, and calls back into `dllFuncs2`. |
| `output/` | Command validation, print routing decisions, alert classification, end-section plans. | `Cbuf_*`, `Cmd_*`, `Con_*`, `Log_*`, `SV_ClientPrintf()`, credits, and disconnect effects. |
| `string_pool/` | String processing, deduplication, static/dynamic accounting, overflow rewind plans, invalid-handle behavior. | `globalvars_t::pStringBase`, 64-bit near-DLL allocation, and physics extension overrides. |
| `changelevel/` | Changelevel queue admission, landmark truncation, duplicate suppression, entity-patch write plans. | Runtime save/load streams, serializer callbacks, token tables, filesystem writes, and level transition execution. |
| `world_queries/` | Trace/visibility admission, fallback decisions, result conversion plans. | BSP traversal, hull selection, leaf storage, PVS/PAS masks, and collision work. |
| `movement/` | Yaw/pitch stepping, walkmove vector planning, fake-client run-command admission, maxspeed plans. | `SV_RunCmd()`, `playermove_t`, PMove setup, physics callbacks, trace execution, and entity relinking. |

The submodule names are allowed to be more granular internally, but avoid a
single broad `game_dll_adapter.cpp`. The bridge is too wide for one adapter to
stay readable.

## Cross-Callback Tests Needed Before Regrouping

The current narrow tests are useful and should remain. Before consolidating
adapters, add concept-level tests that verify interactions across callbacks:

| Test group | Required coverage | Why it matters |
| --- | --- | --- |
| Callback-table routing | Every `enginefuncs_t` slot still maps to the same domain, adapter owner, readiness class, and table order after any regrouping. | Prevents ABI drift while files move. |
| Message bridge aggregate | `pfnMessageBegin()` plus write primitives plus `pfnMessageEnd()` with registered and rewritten messages, including overflow clearing and size mismatch behavior. | Validates the game-DLL message state machine rather than only isolated write helpers. |
| Build-sound composition | `pfnBuildSoundMsg()` style flow: begin message, sound payload decision, end message, destination choice. | This callback intentionally composes messaging and sound payload policy. |
| Active user-message resend | `pfnRegUserMsg()` registration plan plus `SV_SendUserReg()` payload plan plus multicast destination decision. | User-message registration is both registry policy and server messaging. |
| Output-to-message routing | `pfnClientCommand()` stufftext and `pfnClientPrintf()` centerprint routing through message payload writers, including fake-client and non-client skips. | Output callbacks often become network messages. |
| Entity spawn sequence | classname string handling, key-value filtering, `angle` rewrite, `pfnKeyValue()` ordering, `pfnSpawn()` outcome, and custom-entity fallback planning. | Entity parsing is not just a parser; callback ordering is gameplay compatibility. |
| Entity private-data lifecycle | allocation rounding, destructor presence, free ordering, and bugcompat edict-index behavior across create/free helpers. | Prevents losing old mod destructor and pointer quirks. |
| String pool plus entity callbacks | `pfnAllocString()` / `SV_MakeString()` handles used by entity creation, model lookup, and message string writes. | String handles cross many callback groups. |
| Changelevel/save bridge | `pfnChangeLevel()`, queued changelevel state, entity patch intent, and save/restore callback sequencing. | Changelevel touches save compatibility and game callback ordering. |
| Trace/visibility/movement boundary | trace result conversion, PVS/PAS admission, `pfnWalkMove()`, `pfnDropToFloor()`, and fake-client movement gates with synthetic fixtures. | These callbacks cannot be validated in isolation once grouped. |

Phase 122 should pick one of these groups. The lowest-risk candidate is the
message bridge aggregate because the project already has message-session,
user-message, payload, multicast, and envelope tests.

Phase 122 outcome: `game_dll_message_bridge` adds the first tiny aggregate
helper for user-message begin requests and active registration resend payloads.
`tests/engine/game_dll_message_bridge.cpp` now covers fixed and variable user
messages, active registration resend payloads, size-mismatch clearing,
multicast destination planning, and rewrite admission without moving live
`sv.multicast` or callback publication.

## `sv_game.c` Regions Too Coupled To Move Now

These regions should remain in `sv_game.c` or nearby legacy adapters until
their dependencies have a real owner:

| Region | Reason to keep legacy-owned |
| --- | --- |
| `gEngfuncs` concrete table and `BUGCOMP_PENTITYOFENTINDEX_FLAG` slot swap | ABI table order and bugcompat callback replacement are observable by loaded game DLLs. |
| `SV_LoadProgs()` / `SV_UnloadProgs()` | Real library lifetime, command/cvar unlinking, physics API, globals setup, string pool setup, edict allocation, and error cleanup are intertwined. |
| `GiveFnptrsToDll()` publication | This is the actual ABI handshake. It should move only in a final bridge shell phase. |
| `svgame.edicts`, `pvPrivateData`, and private-data destructors | Memory layout and destructor timing are gameplay compatibility. |
| `globalvars_t::pStringBase` and near-DLL string storage | External DLLs may keep `string_t` offsets and expect legacy memory placement. |
| `SV_ParseEdict()` / `SV_LoadFromFile()` callback ordering | Map parse order, utility key filtering, `pfnKeyValue()`, `pfnSpawn()`, and allocation fallback all interact. |
| `sv.multicast`, reliable datagram selection, and `SV_Multicast()` | Destination choice depends on live clients, visibility masks, group filters, reliable buffers, and overflow behavior. |
| Trace, visibility, hull, and PVS/PAS work | These depend on world storage, BSP models, leaf arrays, and collision fixtures that are not yet modern-owned. |
| Movement and fake-client execution | `SV_RunCmd()`, PMove, `sv.current_client`, traces, relinking, and physics callbacks remain broad runtime behavior. |
| Save/restore serializer callbacks | Runtime streams, field descriptors, entity references, and token tables are still legacy-owned. |
| Console, command, cvar, and log sinks | The output policy can be modern, but the live sinks belong to command/cvar/console phases. |

## Recommendation

Do not physically move the helper files in Phase 121. First add one aggregate
test in Phase 122, preferably for the game DLL message bridge, then decide in
Phase 123 whether a small grouped messaging adapter improves readability.

Once aggregate tests exist, move helpers into `game_dll/` one submodule at a
time. File movement should be paired with include-path updates and no behavior
changes, then followed by a separate route-through or adapter-shrink phase.
