# Legacy Engine Survey

A wide-and-shallow first pass over the legacy Xash3D FWGS codebase, produced to
inform the `xash3dpp/` architecture decisions. Each file in this folder is a
1-page summary of one area: what it contains, what it owns, what it depends on,
and where the redesign boundaries are likely to be.

These notes describe the **legacy** code at the repository root. They are a
behavioural reference, not a design constraint for the rewrite.

## Subsystem Summaries

- [engine-common-and-platform.md](engine-common-and-platform.md) — host loop, cmd/cvar, common services, OS abstraction
- [engine-client.md](engine-client.md) — `cl_*`, parse, sound, input
- [engine-server.md](engine-server.md) — `sv_*`, game DLL bridge
- [filesystem.md](filesystem.md) — virtual filesystem, archive backends
- [renderers.md](renderers.md) — `ref/` GL/GLES/software renderers
- [public-common-sdk.md](public-common-sdk.md) — `public/`, `common/`, `pm_shared/` utilities and SDK headers
- [launcher-and-android.md](launcher-and-android.md) — `game_launch/` and `android/` wrappers

## Deep dives

Where the 1-page summaries above are wide-and-shallow, deep dives are
narrow-and-exact: per-topic briefs with struct layouts, algorithms, exact
constants, and quirk catalogues, produced by recon agents immediately before
a chunk is implemented. **Convention:** recon agents targeting a chunk write
their brief here before implementation starts, so future agents (and future
sessions) read instead of re-deriving.

- [deep-dive-bsp-loader.md](deep-dive-bsp-loader.md) — BSP v29/v30/BSP2 on-disk format, `mod_bmodel.c` load path, hull construction, map CRC, 18-item quirk list (Chunk 5 recon, 2026-07-04)
- [deep-dive-trace-pvs.md](deep-dive-trace-pvs.md) — `pm_trace.c` hull-trace kernel (exact pseudocode + epsilons), contents/PVS query surface, map_loader-vs-server split, fixture guidance (Chunk 5 recon, 2026-07-04)
- [deep-dive-delta-encoder.md](deep-dive-delta-encoder.md) — `net_encode.c` delta tables, DT_* flags + wire widths, delta.lst grammar, field-codec math, Xash vs GoldSrc wire dialects, Delta_AddEncoder, baselines (retroactive networking recon, reconstructed 2026-07-04; primary input for the Chunk 6 server)
- [deep-dive-server-lifecycle.md](deep-dive-server-lifecycle.md) — `sv_main/sv_init/sv_cmds`: spawn/activate/deactivate ordering, Host_ServerFrame loop, cvar roster, console commands (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-game-dll-bridge.md](deep-dive-server-game-dll-bridge.md) — `sv_game.c` + eiface/edict/progdefs: enginefuncs_t catalogue, DLL load negotiation, edict lifecycle, string pool, user messages, 32-item bug-compat list (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-clients.md](deep-dive-server-clients.md) — `sv_client/sv_custom/sv_query/sv_filter/sv_log`: connection state machine, usercmd parsing, resource/consistency/download system, queries/rcon/bans/logging (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-physics.md](deep-dive-server-physics.md) — `sv_phys/sv_move/sv_pmove`: MOVETYPE dispatch, pushers, monster locomotion, pmove bridge + unlag, physics-interface hooks (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-world-frame.md](deep-dive-server-world-frame.md) — `sv_world/sv_frame`: areanodes, trace composition, PVS/PHS multicast, snapshot/delta pipeline (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-save-boundary.md](deep-dive-server-save-boundary.md) — `sv_save.c` boundary with the server core: changelevel ordering, game-DLL save callbacks, SAVERESTOREDATA ABI, separation verdict input (Chunk 6 recon, 2026-07-04; format internals deferred to Chunk 8)

## Top-level reference

- [overview.md](overview.md) — cross-cutting map: how the subsystems plug together at the engine level

## Boundary-spec targets

The following subsystems are candidates for a full `/analyse-subsystem` run, in
recommended order (each spec may reference those above it).

### Foundation (no engine deps)

1. `public-utilities` — crtlib, matrixlib, crclib, miniz, utflib, atlas, getopt
1. `filesystem` — archive backends, search paths, plugin ABI
1. `platform` — OS abstraction: time, sleep, DLL load, dialogs, clipboard (`engine/platform/`)

### Core services

1. `memory` — zone allocator, memory pools (`zone.c`)
1. `cmd-cvar` — command buffer, console variable registry (`cmd.c`, `cvar.c`, `base_cmd.c`)
1. `networking` — socket I/O, Netchan reliability/fragmentation, HTTP (`net_ws.c`, `net_chan.c`, `net_buffer.c`)
1. `host` — main frame loop, game state machine, feature flags (`host.c`, `host_state.c`)

### Content pipeline

1. `content-loaders` — model/image/sound format parsers (`imagelib/`, `soundlib/`, `mod_studio.c`, `mod_alias.c`, `mod_bmodel.c`, `mod_sprite.c`)
1. `world-collision` — BSP spatial queries, entity linking, trace (`sv_world.c`, `pm_trace.c`)

### Simulation

1. `physics-pmove` — `pm_shared/`, `sv_phys.c`, `sv_move.c`, `cl_pmove.c` — shared client/server movement
1. `server` — `sv_*`, game DLL bridge (`eiface.h` implementation), edict management
1. `save-restore` — `sv_save.c` binary save format

### Rendering

1. `renderer` — `ref/` plugin ABI, GL/soft backends, studio/BSP draw paths

### Client

1. `client-state` — `cl_main`, `cl_frame`, entity/delta management
1. `client-prediction` — `CL_CreateMove`, prediction loop coupling to server physics
1. `sound` — `engine/client/sound/` (S_Init, mixing, voice)
1. `input` — `engine/client/input/` (keyboard, mouse, gamepad, gyro, touch)
1. `console-ui` — console draw, VGUI bridge, MainUI DLL
1. `demo` — `cl_demo.c` recording/playback format

### Launcher

1. `launcher` — `game_launch/` bootstrap exe + `android/` JNI wrapper
