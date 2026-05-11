# Server Header Boundary Audit

Phase: 143

## Purpose

`engine/server/server.h` is still the main server dependency hub. This audit
classifies what it owns, identifies behavior already represented by modern
private helpers, and proposes a split plan that preserves current include and
layout behavior.

No header split is performed in this phase.

## Current Shape

`server.h` is 705 lines and currently serves four roles:

1. Heavy dependency include for server, common, client, and platform code.
2. Layout owner for core server runtime structs.
3. Global registry for server globals and server cvars.
4. Declaration hub for most `SV_*`, `Log_*`, and game-DLL bridge functions.

It is included by every legacy `engine/server/sv_*.c` file and by non-server
files such as `engine/common/host.c`, `engine/common/model.c`,
`engine/common/net_ws.c`, `engine/client/dll_int/cl_gameui.c`,
`engine/client/parse/cl_parse_gs.c`, and platform library files. That reach is
the main reason a direct split would have noisy build fallout.

## Content Classification

| Area | Examples | Classification | Split risk |
| --- | --- | --- | --- |
| Heavy includes | `edict.h`, `eiface.h`, `physint.h`, `mod_local.h`, `pmove.h`, `protocol.h`, `netchan.h`, `custom.h`, `world.h` | Dependency surface | High until per-source include needs are measured. |
| Private constants and flags | `SVF_*`, `MAP_*`, `GROUP_OP_*`, `MAX_PUSHED_ENTS`, `MAX_VIEWENTS`, `MAX_LOCALINFO_STRING`, `MAX_ENT_LEAFS`, `FCL_*` | Private server constants | Low to medium; many have modern mirrors already. |
| Update/spawn constants | `SV_UPDATE_BACKUP`, `SV_UPDATE_MASK`, `SV_SPAWN_TIME` | Runtime/build-sensitive constants | Medium because `SV_UPDATE_BACKUP` can be a macro or extern depending on `XASH_LOW_MEMORY`. |
| State enums | `sv_state_t`, `cl_state_t`, `cl_upload_t` | Layout-sensitive runtime enums | Medium; client slot helpers mirror some values but legacy structs still use these enums. |
| Core layout structs | `server_t`, `server_static_t`, `sv_client_t`, `svgame_static_t`, `client_frame_t` | Layout-sensitive internal ABI | High; these embed buffers, protocol state, edicts, game DLL tables, resources, and cvar-driven runtime state. |
| Small structs | `sv_baseline_t`, `server_log_t`, `sv_user_message_t`, `sv_pushed_t`, `sv_interp_t` | Domain-owned support layouts | Medium; good future split candidates when grouped by owner. |
| Runtime globals | `svs`, `sv`, `svgame`, `sv_areanodes` | Global state declarations | High; many call sites directly mutate fields. |
| Cvar externs | `sv_unlag`, `sv_maxrate`, `sv_voiceenable`, `sv_autosave`, `skill`, `deathmatch`, and related server cvars | Runtime configuration registry | Medium; likely split into `server_cvars.h` first. |
| Function declarations | `SV_*`, `Log_*`, `pfn*` declarations grouped by legacy source file | Declaration hub | Medium; can split by old source owner after callers are measured. |
| Inline helpers | `SV_ModelHandle`, `SV_HavePassword`, `SV_IsPlayerIndex`, `SV_CheckEdict`, `SV_EdictNum` | Header-only accessors over globals/layout | High for edict/model helpers; low for player/password predicates if replaced by pure helpers first. |

## Modern Coverage Already In Place

The modern tree already covers many `server.h` constants and behavior seams,
but mostly as target-neutral helpers rather than replacement declarations.

| Legacy area | Modern coverage |
| --- | --- |
| Server-only constants and flags | `src/include/engine/server/server_limits.hpp`, with descriptor metadata in `server_limits.cpp`. |
| Maxclient/update-backup/entity capacities and spawn settling | `server_lifecycle_limits.hpp`. |
| Map validation flags and changelevel landmark behavior | `server_map_validation.hpp`. |
| Group operation predicates | `server_group_filter.hpp`. |
| `FCL_*` client flags, userinfo penalty, rate/update interval, prediction/lag/local weapon decisions | `client/client_policy.hpp`. |
| Client slot states, first-free slot, and master heartbeat population decisions | `client/client_session_slots.hpp`. |
| Connectionless command classification, challenge/reject responses, user agent policy, rcon policy | `client/connectionless_classifier.hpp`, `client/connection_response.hpp`, `client/user_agent_policy.hpp`, `client/remote_admin_command.hpp`, `client/server_challenge_policy.hpp`. |
| Resource and customization flows | `resources/resource_identity.hpp`, `resources/resource_transfer_manifest.hpp`, `resources/server_resource_catalog.hpp`, `resources/server_download_policy.hpp`, `resources/server_upload_queue.hpp`, `resources/server_consistency_*`, `resources/server_hot_resource.hpp`, `resources/server_reslist_policy.hpp`. |
| Message payloads and recipients | `messaging/server_message_envelope.hpp`, `messaging/server_*_message.hpp`, `messaging/server_multicast_policy.hpp`, `messaging/server_event_playback_policy.hpp`, `messaging/server_packet_entities_delta.hpp`, `messaging/server_frame_datagram.hpp`, `messaging/server_voice_relay.hpp`. |
| Game DLL bridge decisions | `game_dll/game_dll_*` headers for load/unload, entity lifecycle, entity parse, output, payloads, resources, message session, user-message registry, movement, visibility, and changelevel. |
| World/physics/PMove constants and narrow decisions | `server_world_link_policy.hpp`, `server_physics_routing_policy.hpp`, `server_movement_constraints.hpp`, `server_visibility_constraints.hpp`, `server_pmove_bridge_policy.hpp`. |
| Save/restore format and pure decisions | `save_restore_format.hpp`, `save_restore_values.hpp`. |
| Server command/operator policies and event log formatting | `server_command_lifecycle.hpp`, `server_operator_command_policy.hpp`, `server_event_log.hpp`. |

These modern helpers reduce future pressure on `server.h`, but they do not yet
replace the core layout structs, globals, or function declarations.

## Proposed Split Plan

Keep `server.h` as the compatibility facade while introducing smaller private
headers underneath it. Each new header should be included by `server.h` first,
then individual `.c` files can migrate to narrower includes when safe.

Suggested order:

1. `server_constants.h`
   Move private constants and flags that already have modern mirrors:
   `SVF_*`, `MAP_*`, `GROUP_OP_*`, `MAX_*`, `FCL_*`, and `SV_SPAWN_TIME`.
   Leave `SV_UPDATE_BACKUP` / `SV_UPDATE_MASK` until the low-memory extern vs
   macro behavior is explicitly tested.

2. `server_cvars.h`
   Move the `extern convar_t` declarations. This would let non-server files
   such as `net_ws.c` include a cvar-only header instead of all server state.
   Keep definitions and registration ownership in legacy source files.

3. `server_forward.h`
   Add forward declarations for `server_t`, `server_static_t`, `sv_client_t`,
   `svgame_static_t`, and other common server-owned structs where full layout
   is not needed. This should be caller-driven, not guessed.

4. `server_decls/*.h` or per-domain private declaration headers
   Split function declarations by owner only after call sites are known:
   `sv_main`, `sv_client`, `sv_game`, `sv_custom`, `sv_world`, `sv_save`,
   `sv_pmove`, `sv_log`, and `sv_query`.

5. `server_state.h`
   Move layout structs and global externs only after narrower constants/cvar
   headers have reduced non-server includes. This is the riskiest split because
   it pulls in nearly every heavy dependency.

6. `server_inline.h`
   Move or replace inline helpers only after their dependencies are clear.
   `SV_CheckEdict()` and `SV_EdictNum()` should stay with `svgame.edicts` layout
   until edict storage ownership is better isolated.

## Defer List

Do not split these yet:

- `server_t`, `sv_client_t`, `svgame_static_t`, and `server_static_t`;
- `svs`, `sv`, `svgame`, and `sv_areanodes` globals;
- edict/model inline helpers that read `svgame.edicts`, `sv.models`, or `GI`;
- game DLL callback tables and edict storage declarations;
- world trace, hull, and PMove declarations that still rely on broad legacy
  structs;
- save/restore runtime declarations that still mutate `SAVERESTOREDATA`,
  `.HL?` files, game DLL field callbacks, and transition state.

## Recommended Next Split Candidate

The safest first real split is a constants-only compatibility header:

```text
engine/server/server_constants.h
```

`server.h` would include it, so public include behavior remains unchanged.
Only after that builds cleanly should individual modern adapters or legacy
files include `server_constants.h` directly.

The second candidate is:

```text
engine/server/server_cvars.h
```

That split can reduce broad `server.h` includes in common/platform code that
only needs `sv_cheats`, `sv_nat`, `sv_password`, or similar cvars.

## Validation

This phase is documentation-only. Validation should be `git diff --check` and
`scripts/phase-status.ps1 -PhaseNumber 143`.
