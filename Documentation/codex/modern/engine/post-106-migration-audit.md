# Post-106 Engine Migration Audit

This audit follows the server constants and constraints lane. It looks for
small modern ownership points that can unlock more future migration than simply
extracting one more server helper.

## Current Position

The modern tree already has a large server helper surface under
`src/engine/server`. Most of it is target-neutral policy or payload building:

- game DLL bridge policies, message sessions, entity parsing, lifecycle,
  string-pool compatibility, payloads, resource callbacks, visibility, trace,
  movement, changelevel, and fake-symbol load planning;
- resource identity, catalogs, download/upload queues, consistency checks,
  customizations, hot resources, reslists, and resource messages;
- source-query and NetAPI response building, connectionless classification,
  connection responses, user-agent decisions, client command dispatch, client
  rate/userinfo policy, server filters, event logs, frame datagrams, multicast,
  text/service/static/sound/voice payloads, spawn handshakes, and save-format
  fixtures;
- low-level shared engine helpers for commands, command buffers, info strings,
  network bit buffers, platform command-line lookup, system-console output,
  filesystem mount flags, hashes, checksums, CRT path/text/conversion helpers,
  atlas data, and build numbers.

That surface gives us good tests and vocabulary, but the live runtime still
leans on legacy global state. The large remaining files are still large for
real reasons: `sv_game.c`, `sv_client.c`, `sv_save.c`, `sv_phys.c`,
`sv_world.c`, `sv_main.c`, `sv_frame.c`, and shared `engine/common` services
still own ABI calls, command/cvar mutation, console output, allocation, packet
delivery, edict/client/server storage, BSP/model state, and save/load streams.

This stage is intentionally adapter-heavy. It should not become the final
architecture. The next server work should still use small compatibility
adapters, but later consolidation passes must regroup related helpers into real
concepts instead of leaving one C++ file per old C function. See
`Documentation/codex/modern/cpp-ownership-target.md` for the project-wide
target.

## Gravity Wells

The scan found a few recurring dependencies that make the next migrations more
adapter-heavy than they need to be.

### Group Filtering

`GROUP_OP_AND`, `GROUP_OP_NAND`, `svs.groupop`, and `groupinfo` checks appear
in `sv_game.c`, `sv_world.c`, `sv_pmove.c`, `sv_phys.c`, `sv_move.c`, and
`sv_save.c`. They affect touch admission, collision, player movement,
multicast/event filtering, and save/restore context.

This is a good next enabler because the core predicate is small and testable:
given an operation and two group masks, decide whether the candidate passes.
The actual edict iteration, PVS/PHS masks, traces, and save code should stay
legacy-owned.

### Map Validation Flags

`MAP_IS_EXIST`, `MAP_HAS_LANDMARK`, and `MAP_INVALID_VERSION` are used by
`SV_MapIsValid()`, changelevel, command lifecycle, and save/load admission.
The modern tree already has changelevel policy and command lifecycle helpers,
but flag interpretation is still spread across `sv_game.c`, `sv_cmds.c`, and
`sv_save.c`.

The next step should not move map probing or entity parsing. It should add a
small flag/result interpretation helper so legacy call sites can share the same
"invalid", "missing", "landmark missing", and "usable" decisions.

### Client Flags

`FCL_*` remains widely shared. The biggest users are:

- `FCL_FAKECLIENT`: connection, query, movement, command, frame, customization,
  and game DLL paths;
- `FCL_HLTV_PROXY`: frame, customization, query, and multicast/event paths;
- `FCL_LOCAL_WEAPONS`, `FCL_PREDICT_MOVEMENT`, and
  `FCL_LAG_COMPENSATION`: movement, prediction, client data, and event filtering;
- resend/send/skip flags: frame datagram ownership;
- resource/consistency flags: customization and download ownership.

This is not a good broad replacement target. The safe move is to create typed
snapshots and small predicates by owner, then route one owner at a time.

### Event Playback

`SV_PlaybackEventFull()` in `sv_game.c` mixes event flags, invoker validation,
PVS/PHS masks, group filtering, client flags, recipient iteration, reliable
versus unreliable routing, and payload construction. It is a natural future
target, but it depends on group filtering and client flag predicates first.

### Cvar And Command Access

The modern BaseCmd and command-buffer work helped, but live server and game DLL
paths still call `Cvar_*`, `Cmd_*`, and `Con_*` directly. A full cvar rewrite is
too large right now. A smaller cvar read snapshot/query helper would reduce
adapter friction for server helpers that only need typed values.

### Model, BSP, And Visibility

`engine/common/mod_bmodel.c`, `sv_world.c`, `sv_phys.c`, and game DLL trace
callbacks remain coupled through PVS/PAS, hulls, traces, and edict leaves. This
area is important, but it is not the next low-risk lane. It should get an audit
before implementation.

## Recommended Immediate Lane

The next route should be enabler-first:

1. server group filter policy;
2. map validation and landmark result policy;
3. client flag accessor/predicate policy;
4. event playback boundary audit;
5. event playback recipient/admission helper;
6. read-only cvar snapshot pilot.

This order lets the event and game DLL paths use smaller, tested concepts
instead of growing one-off adapters.

## Deferred For Later

- Broad memory-pool modernization remains Phase 990. It can unlock cleaner
  ownership later, but changing allocation now would increase risk before more
  C++ ownership boundaries exist.
- Runtime save/restore migration should wait until map validation, entity parse,
  string-pool compatibility, and field serialization boundaries are stronger.
- Model/BSP/PVS ownership needs a dedicated audit and probably several fixture
  tests before any route-through.
- Rendered-console routing should wait for a client/render console phase. System
  console work does not automatically solve the in-game console sink.
- Non-Windows runtime work stays in the 800-series validation phases.

## Practical Next Steps

Start with group filtering. It is small, repeated, and behavior-sensitive. It
also directly supports later event playback and trace/collision cleanup.

After that, use map validation flags and client flag predicates to reduce the
amount of bit-mask knowledge that future helpers need to carry. Once those
pieces exist, the server event playback lane becomes a much cleaner target than
it is today.

After the next few enabler phases, schedule a consolidation checkpoint. That
checkpoint should decide which helpers are now part of a larger domain concept,
which files should move or merge, and which adapters can shrink.
