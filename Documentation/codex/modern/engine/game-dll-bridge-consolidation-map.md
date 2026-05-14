# Game DLL Bridge Consolidation Map

Phase 152 re-scans `engine/server/sv_game.c` against the modern
`src/engine/server/game_dll/` helpers. The game DLL bridge is a high-risk
boundary because it publishes the `enginefuncs_t` callback table to external
game DLLs, owns DLL load/unload ordering, and still works directly with live
edict, client, message-buffer, and server state.

## Current Domains

| Domain | Modern helpers | Legacy-owned boundary |
| --- | --- | --- |
| ABI metadata and load policy | `game_dll_enginefuncs`, `game_dll_load_policy` | `gEngfuncs` table order, `GiveFnptrsToDll`, `GetEntityAPI*`, `GetNewDLLFunctions`, `SV_LoadProgs`, `SV_UnloadProgs` |
| Entity lifecycle and parsing | `game_dll_entity_lifecycle`, `game_dll_entity_parse` | edict array storage, private-data pointers, game DLL constructor/destructor callbacks, `pfnSpawn`, map entity iteration |
| Messaging and user messages | `game_dll_message_bridge`, `game_dll_message_session`, `game_dll_user_message_registry` | `sv.multicast`, `sizebuf_t`, user-message table mutation, multicast send timing, message begin/write/end call order |
| Resources and precache | `game_dll_resource_policy`, resource catalog helpers | model/sound/generic/event/decal arrays, filesystem probes, fatal precache errors, resource list mutation |
| Payload side effects | `game_dll_payload_policy` | baseline capture, temp entity writes, particle writes, static decal/entity emission, sound routing call sites |
| Client info and userinfo | `game_dll_client_info_policy` | live client slots, info-string mutation, cvar query dispatch, auth/userid storage |
| Movement and visibility | `game_dll_movement_policy`, `game_dll_visibility_trace_policy` | world traces, hull selection callbacks, PMove state, PVS/PAS storage, `playermove_t` population |
| Output and commands | `game_dll_output_policy` | `Cbuf_*`, console sinks, client prints, log sinks, disconnect and credits side effects |
| Changelevel | `game_dll_changelevel_policy` | map existence probes, landmark entities, queued changelevel mutation |
| String pool compatibility | `game_dll_string_pool_compat` | legacy string handles, allocation storage, string table lifetime |

## Consolidation Candidates

Safe near-term candidates are adapters that only translate plain scalar values:

- modern enum class to legacy C `int`;
- legacy `int` truthiness to `bool`;
- `bool` fields back to legacy `0`/`1`;
- fixed-size legacy string copies already guarded by resource adapter helpers.

These can be shared without hiding runtime ownership because they do not call
the game DLL, mutate buffers, access edicts, read the filesystem, or change
callback publication order.

## Boundaries To Keep Legacy-Owned

The following remain off-limits for broad consolidation until fixture coverage
is stronger:

- `gEngfuncs` slot order and ABI publication;
- DLL load, API negotiation, shutdown, and unload ordering;
- edict storage, private-data lifetime, and game DLL destructor calls;
- live `sizebuf_t` and multicast/reliable datagram ownership;
- user-message table mutation and registration resend timing;
- filesystem/resource table mutation during precache;
- trace, PVS/PAS, and PMove runtime state;
- string-pool allocation storage and handles.

## Phase 153 Target

The first shrink target is scalar game DLL adapter glue. Client-info and output
adapters already depend on modern enum ordinals matching legacy C constants.
Phase 153 makes that dependency explicit with tests and `static_assert`s, then
routes repeated bool/enum conversions through one tiny shared helper.
