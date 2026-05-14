# Milestone 100 Server Modernization Audit

This audit marks the end of the first long game-DLL bridge lane and looks for
the next low-risk migration targets in the server area.

## Current Modern Coverage

`src/engine/server` now contains target-neutral helpers for several server
domains:

- game DLL bridge metadata, message sessions, user messages, output,
  resources, payloads, client info, string-pool fixtures, entity lifecycle,
  entity parsing, changelevel/save intent, visibility/trace admission,
  movement policy, and fake-symbol DLL load planning;
- server resource identity, catalogs, downloads, uploads, consistency checks,
  customizations, hot resources, reslists, and resource messages;
- server client policy, client command dispatch, connectionless
  classification, connection responses, NetAPI/source-query payloads, user
  agent policy, filters, event logs, command lifecycle, spawn handshakes,
  frame datagram routing, multicast routing, text/service/static/sound/voice
  payloads, and save/restore file-format fixtures.

Most of those helpers are pure C++ value objects or byte writers with focused
unit tests. Legacy C files generally still own live global state, ABI
callbacks, networking sends, console output, filesystem probes, command/cvar
mutation, entity storage, game DLL calls, and runtime side effects.

## Remaining Legacy Shape

`engine/server` still contains the large live owners:

- `server.h`: shared server globals, client/server structs, flag macros,
  limits, and function declarations.
- `sv_client.c`: connection handshake, fake clients, client command dispatch,
  downloads/uploads, userinfo mutation, client messages, and game DLL client
  callbacks.
- `sv_cmds.c`: operator commands, lifecycle commands, info/localinfo command
  mutation, save/load/changelevel command entry points.
- `sv_custom.c`: customization propagation, consistency setup/response,
  HPAK/resource side effects.
- `sv_filter.c`: ban files and command surface, now partly policy-backed.
- `sv_frame.c`: per-frame packet entity selection, reliable/unreliable
  datagram fanout, client update loops.
- `sv_game.c`: game DLL callback table, string pool, edict lifecycle,
  user-message callbacks, resource callbacks, trace/visibility/movement
  callbacks, and DLL load/unload.
- `sv_init.c`: resource precache/indexing, baselines, spawn/activate/deactivate
  lifecycle, test packet generation.
- `sv_log.c`: server log file lifecycle and print sinks.
- `sv_main.c`: server cvars, connectionless dispatch, challenge/connect flow,
  main server frame and timeout loops.
- `sv_move.c`, `sv_phys.c`, `sv_pmove.c`, `sv_world.c`: world/physics/player
  movement and trace ownership.
- `sv_save.c`: runtime save/load streams and game DLL field serialization.
- `sv_query.c`: source-query entry point, now mostly payload-backed.

This is expected. The modern layer has reduced policy ambiguity, but the live
runtime still depends heavily on legacy global ownership.

## Constants And Constraints Already Mirrored

Some server constraints already exist as modern constants:

- save/restore file magic, versions, heap size, token count, packed short size;
- game-DLL user-message service ranges, payload sizes, and name limits;
- resource message bit widths, hash/reserved sizes, and flags;
- sound/static/service/text/voice message command numbers and bit widths;
- spawn handshake service command numbers and hull component counts;
- frame datagram resend/skip/send flags;
- game DLL movement flags and walkmove modes;
- game DLL visibility hull ranges and leaf capacities;
- changelevel map validation flags;
- consistency-list bit widths and resource identity sizes.

Those mirrors have helped tests stay readable, but they are scattered by
feature. We do not yet have a single server-runtime contract that groups
legacy server-only limits and explains whether each value is ABI, protocol,
save-format, gameplay, or private implementation detail.

## Good Next Lane

The next practical lane is **server constants and constraints**, not a broad
server rewrite. The goal is to create typed, documented modern contracts for
server-only constants first, then route individual users only where that
improves clarity without moving ownership.

Strong first candidates:

- `CHALLENGE_WINDOW_SECONDS` in `sv_client.c`: small, server-only, testable,
  and useful for connection/challenge policy work.
- `MOVE_NORMAL` / `MOVE_STRAFE` in `sv_move.c`: small movement route constants
  already mirrored for game-DLL policy.
- `MOVE_EPSILON` and server `MAX_CLIP_PLANES` in `sv_phys.c`: server physics
  constraints that should be named independently from unrelated GL clip-plane
  constants and pm_shared definitions.
- `SV_SPAWN_TIME`, `SV_UPDATE_BACKUP`, and client-count bounds in `sv_init.c`:
  lifecycle constraints useful before deeper spawn/update routing.
- `MAX_VIEWENTS`, `MAX_PUSHED_ENTS`, `MAX_LOCALINFO_STRING`, and
  `MAX_ENT_LEAFS(ext)` in `server.h`: important server storage constraints,
  but tied to struct layout, so start by mirroring and testing rather than
  replacing macros.

Lower-risk route-through after constants are mirrored:

- challenge time-window calculation;
- update-backup selection and maxclient bounds;
- `MAX_ENT_LEAFS` capacity selection for visibility helpers;
- movement type names and mode admission around `SV_MoveToOrigin`.

Hold for later:

- replacing `server.h` struct-size limits directly;
- moving save/runtime stream constants out of `sv_save.c` at the C call sites;
- physics/world clip loops beyond small policy helpers;
- anything that changes edict/client/server struct layout.

## Proposed Next Phases

1. **Phase 101: Server Constants And Constraints Inventory**
   Create a focused TODO and modern `server_limits` contract with tests that
   compare the modern values against legacy constants through a small adapter
   where needed. No route-through yet.

2. **Phase 102: Server Challenge Window Policy**
   Extract the challenge time-window calculation and validation constraints
   from `sv_client.c` while leaving hash generation, salt storage, and packet
   sends legacy-owned.

3. **Phase 103: Server Lifecycle Limits Policy**
   Model maxclient bounds, singleplayer/multiplayer update-backup selection,
   `SV_SPAWN_TIME`, and client entity count calculation before touching spawn
   or activation flow.

4. **Phase 104: Server Movement Constraint Constants**
   Consolidate server-only movement constants and add tests around move type,
   clip-plane limit, and epsilon semantics. Keep actual physics and collision
   loops legacy-owned.

5. **Phase 105: Visibility Leaf And View Constraint Policy**
   Centralize leaf capacity and view-entity limits so game-DLL visibility,
   world linking, and frame packet code can share the same named constraints.

6. **Phase 106: Runtime Route-Through Review**
   After the constraint contracts are tested, decide which tiny legacy call
   sites should route through the modern helpers and which should remain
   macros because they define struct layout or protocol compatibility.

## Recommendation

Proceed with Phase 101. It is low risk, gives us a stable vocabulary for the
next server migrations, and should make later route-through work less ad hoc.
Avoid deleting or replacing legacy macros until tests prove the modern values
match and the affected macro is not part of layout-sensitive storage.
