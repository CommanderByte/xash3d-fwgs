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

## Top-level reference

- [overview.md](overview.md) — cross-cutting map: how the subsystems plug together at the engine level

## Boundary-spec targets

The following subsystems are candidates for a full `/analyse-subsystem` run, in
recommended order (each spec may reference those above it).

**Foundation (no engine deps)**
1. `public-utilities` — crtlib, matrixlib, crclib, miniz, utflib, atlas, getopt
2. `filesystem` — archive backends, search paths, plugin ABI 
3. `platform` — OS abstraction: time, sleep, DLL load, dialogs, clipboard (`engine/platform/`)

**Core services**
4. `memory` — zone allocator, memory pools (`zone.c`)
5. `cmd-cvar` — command buffer, console variable registry (`cmd.c`, `cvar.c`, `base_cmd.c`)
6. `networking` — socket I/O, Netchan reliability/fragmentation, HTTP (`net_ws.c`, `net_chan.c`, `net_buffer.c`)
7. `host` — main frame loop, game state machine, feature flags (`host.c`, `host_state.c`)

**Content pipeline**
8. `content-loaders` — model/image/sound format parsers (`imagelib/`, `soundlib/`, `mod_studio.c`, `mod_alias.c`, `mod_bmodel.c`, `mod_sprite.c`)
9. `world-collision` — BSP spatial queries, entity linking, trace (`sv_world.c`, `pm_trace.c`)

**Simulation**
10. `physics-pmove` — `pm_shared/`, `sv_phys.c`, `sv_move.c`, `cl_pmove.c` — shared client/server movement
11. `server` — `sv_*`, game DLL bridge (`eiface.h` implementation), edict management
12. `save-restore` — `sv_save.c` binary save format

**Rendering**
13. `renderer` — `ref/` plugin ABI, GL/soft backends, studio/BSP draw paths

**Client**
14. `client-state` — `cl_main`, `cl_frame`, entity/delta management
15. `client-prediction` — `CL_CreateMove`, prediction loop coupling to server physics
16. `sound` — `engine/client/sound/` (S_Init, mixing, voice)
17. `input` — `engine/client/input/` (keyboard, mouse, gamepad, gyro, touch)
18. `console-ui` — console draw, VGUI bridge, MainUI DLL
19. `demo` — `cl_demo.c` recording/playback format

**Launcher**
20. `launcher` — `game_launch/` bootstrap exe + `android/` JNI wrapper
