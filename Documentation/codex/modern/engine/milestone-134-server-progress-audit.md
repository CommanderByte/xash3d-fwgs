# Milestone 134 Server Progress Audit

Phase: 135

## Why This Checkpoint Exists

Phases 52 through 134 created a large modern server helper layer while keeping
the legacy server runnable. The work is valuable, but the project has reached a
point where adding one more tiny helper per legacy seam is less useful than
asking whether the helper layer is starting to simplify the architecture.

The short answer is:

- behavior knowledge and test coverage are much better than before;
- compatibility risk is still contained behind legacy `SV_*` call sites;
- the modern server layer is not yet the runtime owner;
- the next phases should consolidate related helpers into domains and shrink
  adapter glue before extracting many more small policies.

## Current Shape

Repository scan on 2026-05-11:

| Area | Count / size | Meaning |
| --- | ---: | --- |
| Modern server `.cpp` files | 63 files, about 10.2k lines | Mostly target-neutral value objects, policy helpers, byte writers, and fixture parsers. |
| Modern server `.hpp` files | 63 files, about 4.9k lines | Private C++ contracts used by focused tests and legacy adapters. |
| Server adapter files | 104 `.cpp` / `.h` files, about 7.1k lines | Compatibility translation between `server.h`, globals, packet buffers, cvars, commands, filesystem, and modern helpers. |
| Legacy non-adapter server files | 16 files, about 24.3k lines | The live runtime still lives here. |
| Focused engine tests | 74 C++ tests, about 12.7k lines | Strong behavior net for the helper layer. |

The high adapter count is expected for this migration style. The issue is not
that adapters exist; the issue is that repeated adapter patterns should not
become permanent architecture.

## What Is Already Covered Well

The modern server layer now has useful coverage in these areas:

- resource identity, resource catalog rows, manifests, downloads, uploads,
  customizations, consistency, hot resources, and `.res` token policy;
- server messages, voice, sound, static entities, text/service messages,
  multicast recipient decisions, envelopes, frame datagram gates, and packet
  entity cursor planning;
- game DLL bridge metadata, load policy, entity lifecycle, entity parsing,
  string pool compatibility, user messages, message sessions, output,
  resources, payloads, movement callbacks, visibility/trace admission, client
  info, and changelevel intent;
- client/session policy, command dispatch, connectionless classification,
  connection/rejection responses, challenge windows, user-agent validation,
  source query and NetAPI payloads, remote-admin command policy, and client
  slot population;
- server constants, visibility constraints, group filtering, map validation,
  runtime timeout policy, operator command argument policy, event playback,
  world link splitting, physics routing, PMove unlag timing, and save-format
  fixtures.

This is the good news: much of the ambiguous server behavior is now named,
tested, and documented.

## What Is Still Legacy-Owned

The following areas are not meaningfully modernized yet, even when small helper
calls exist:

- `sv`, `svs`, `svgame`, `server_t`, `server_static_t`, `sv_client_t`,
  `edict_t`, `ENTITYTABLE`, and `SAVERESTOREDATA` storage and lifetime;
- command/cvar registration, mutation, callbacks, and console output;
- filesystem probes, HPAK access, save-slot files, logs, screenshots, and
  temporary save extraction;
- network packet reads, `sizebuf_t` mutation, `MSG_*` writers, netchan sends,
  reliable/unreliable datagram ownership, and signon buffer mutation;
- game DLL and physics extension callback ordering;
- renderer/audio restore side effects;
- world area nodes, edict links, BSP visibility, hull selection, exact traces,
  trace globals, trigger callbacks, and water/content tests;
- PMove setup/finish, physent population, callback table publication, touch
  replay, and command execution;
- live save/load streams, entity creation/restore, global entity merge,
  landmark transition effects, and map spawn/activation flow.

These are the hard parts. The modern layer has reduced uncertainty around
them, but it has not replaced their ownership.

## Progress Assessment

The engine server is roughly in the **late compatibility-facade / early
behavior-ownership stage** from `Documentation/codex/modern/cpp-ownership-target.md`.

Good signs:

- Helpers usually use plain values and explicit result types.
- Focused tests describe many compatibility quirks.
- Modern code mostly avoids including `server.h`.
- Repeated policy decisions are named rather than hidden in large C functions.
- Runtime smoke tests have stayed green across many server phases.

Warning signs:

- The flat `src/engine/server` folder now has enough files that ownership is
  harder to see at a glance.
- Many helpers still mirror extraction order rather than final domain shape.
- Adapter count is high and several adapters repeat similar buffer/result
  translation patterns.
- Some docs now describe planned submodules, but the code is still physically
  flat.
- The legacy runtime still owns almost every mutable server surface.

The project is not stalled, but it should now pivot from pure extraction to
consolidation.

## Sensible Simplifications

These are worth doing soon:

1. **Add domain indexes before moving files.**
   Create explicit module maps for resources, messaging, game DLL bridge,
   client/session, world/physics, save/restore, and runtime shell. This avoids
   moving files into attractive folders before deciding ownership.

2. **Consolidate modern helpers by domain.**
   Start with resources and messaging because they already have aggregate
   tests and shared vocabulary. Move or group files only when the domain name
   is clear.

3. **Shrink adapter repetition, not adapter count blindly.**
   Shared adapter helpers already exist for resource and message code. Extend
   that pattern where it removes repeated byte-buffer or snapshot conversion
   glue.

4. **Introduce small domain facades only where they clarify intent.**
   A facade such as `ResourceTransferManifest` is useful because it explains a
   flow. A broad `ServerEverything` facade would hide ownership and should not
   exist.

5. **Use first-class value objects for save/runtime policy next.**
   Save admission, save comment/version classification, and save archive
   manifests are better next steps than replacing runtime save streams.

6. **Prepare fixtures before touching world, trace, PMove, or save runtime.**
   These areas need synthetic edict/client/world fixtures or golden runtime
   captures before meaningful ownership can move.

These are not worth doing yet:

- splitting `server.h` aggressively;
- moving `sv_game.c`, `sv_client.c`, `sv_world.c`, `sv_phys.c`, `sv_pmove.c`,
  or `sv_save.c` wholesale;
- merging all adapters into one file;
- exposing modern C++ types across game DLL or public C boundaries;
- rewriting exact trace, PMove, or save streams without fixtures.

## Documentation Hygiene

The active phase list now points at Phase 136 as the next open phase. The
parked future buckets remain Phase 800+, Phase 990, Phase 1000, and Phase 1100.

Older TODO documents still contain a few open items:

- `Documentation/codex/todo/server_migration_todo.md` has broad leftovers for
  server event logging, declarative command registration, runtime save/restore,
  game DLL bridge, and physics/world migration. These are not active drift; the
  new Phase 136-146 lane gives them a clearer path.
- `Documentation/codex/todo/debugging_todo.md` still has deferred sink and
  filesystem capture items. Most of these wait for console routing or
  filesystem debug command ownership.
- `Documentation/codex/todo/file_handle_todo.md` has one later RAII-wrapper
  note. It should stay parked until filesystem file-handle ownership moves
  further.
- `Documentation/codex/todo/engine_deferred_todo.md` still holds the deferred
  filesystem logging callback item, which should wait for a real console
  router/sink phase.

No completed server TODO should be moved to `done/` in this checkpoint. The new
post-134 TODO is intentionally active, and the older broad TODOs still hold
useful deferred context.

## Recommended Next Direction

The next active lane should be **server consolidation**, not more random
server extraction.

Recommended order:

1. plan module layout and ownership rules;
2. pilot resource-transfer grouping;
3. pilot messaging grouping;
4. pilot game DLL bridge grouping;
5. pilot client/session grouping;
6. add first save/restore value objects;
7. inventory and shrink adapter repetition;
8. audit `server.h` dependency boundaries;
9. build world/trace fixtures;
10. build PMove/usercmd fixtures;
11. run a runtime/performance checkpoint.

This keeps the project moving toward the desired end state: modern C++ modules
own internal concepts, while thin legacy facades preserve ABI, protocol, file
format, and mod compatibility.

## Validation

This is a documentation checkpoint. Validation:

- `scripts/phase-status.ps1 -PhaseNumber 135`
- `git diff --check`
