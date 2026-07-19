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

- [deep-dive-utilities.md](deep-dive-utilities.md) — legacy `public/` utility library (crtlib, crclib, matrixlib, xash3d_mathlib, atlas, swaplib, build, utflib, getopt, dllhelpers) + gameinfo text parser: struct/constant/algorithm reference, `CRC32_BlockSequence` and studio-bone quirks, and a legacy→`xash3dpp/utilities` as-built mapping table (as-built refresh, 2026-07-06)
- [deep-dive-memory.md](deep-dive-memory.md) — legacy `zone.c` pool allocator: `memheader_t`/`memheader_small_t`/`mempool_t` layouts, `0xA1BA`/`0xAD1E`/`0xDF` sentinels, `MEM_SMALL_ALLOC_OPT`, the frozen 32-bit `poolhandle_t` rationale, alloc/free/realloc-migrate/check algorithms, `XASH_CUSTOM_SWAP`, and a legacy→`xash3dpp/memory` as-built mapping showing the stats-facade redesign (as-built refresh, 2026-07-06)
- [deep-dive-filesystem.md](deep-dive-filesystem.md) — legacy `filesystem/` VFS + archive backends: `searchpath_t` linked list + 7-fn-ptr vtable, `file_t`/`ztoolkit_t`, PAK (`dpackfile_t` 64B), WAD2/WAD3 (`dwadinfo_t`/`dlumpinfo_t` + `TYP_*`), ZIP/PK3 (LFH/CDF/EOCD, `#pragma pack(1)`), `g_archives[]` mount order + path-flag table, `GetFSAPI` plugin ABI, gameinfo discovery, and a legacy→`xash3dpp/filesystem` as-built mapping (pimpl + `ISearchBackend`, `xash3dpp_miniz` shared target, platform-I/O extraction) (as-built refresh, 2026-07-06)
- [deep-dive-platform.md](deep-dive-platform.md) — legacy `engine/platform/` OS abstraction: the `Platform_*` API + `#ifdef`-mesh backend selection (`XASH_TIMER`/`XASH_LIB`/`XASH_MESSAGEBOX`), `Platform_DoubleTime` first-call epoch, `Win32_NanoSleep` high-res waitable timer, the Win32 custom in-memory PE loader vs POSIX `dlopen` (+ solder/vrtld/dladdr shims), `Wcon_*`/`Posix_Input` console, SEH/`sigaction`+libbacktrace crash, `Posix_Daemonize`/SIGTERM, the `net_ws.c` socket surface relocation, and a legacy→`xash3dpp/platform` as-built mapping (free functions, RAII handles, extracted OS file-I/O + sockets, hosted diagnostics tier) (as-built refresh, 2026-07-06)
- [deep-dive-core.md](deep-dive-core.md) — legacy origins of the `xash3dpp/core` distillation: the `host.c` frame-timing loop (`Host_CalcFPS`/`Host_FilterTime`/`Host_CalcSleep`, `host.realtime`/`frametime`/`realframetime`/`framecount`) and its 7-cvar roster (with the `fps_max`/`sleeptime` public-name drift), the `Con_Printf`/`Con_DPrintf`/`Msg` + `S_ERROR`/`S_WARN`-prefix console family unified into severity-typed `core::log` + the `log_set_callback` G-1 hook, the `ASSERT`→`XASH_ASSERT`/`XASH_FATAL` two-tier split, the `Host_Error`/`Sys_Error` fatal paths, and an honest NEW-vs-distilled as-built mapping flagging the two rewrite-native primitives (structured `ErrorCode`, the `ThreadRole` registry + `assert_thread_role` enforcement) plus the D-1 diagnostics-tier build split (as-built refresh, 2026-07-06)
- [deep-dive-cmd_cvar.md](deep-dive-cmd_cvar.md) — legacy origins of `xash3dpp/cmd_cvar`: the `cmd.c` two-queue 32 KB command buffer + `wait`, the `SV_Active() && SV_GetMaxClients()==1` stuffcmd-trust test now behind `ITrustOracle`, `Cmd_TokenizeString` (`MAX_CMD_TOKENS 80`, quotes, `;`/`\n`, `//`), the frozen `cvar_t`/`convar_t` layouts + `CVAR_SENTINEL` detection (replaced by two explicit registration paths + `FCVAR_DLL_WRAPPER`), the `FCVAR_*` flag/wire semantics, `Cvar_UpdateInfo`'s userinfo/serverinfo circular-dep tail now behind `ICvarObserver`, the HL25 `gl_widescreen_yfov`→`r_adjust_fov` + `HACKS_RELATED_HLMODS`/`CMD_OVERRIDABLE` quirks isolated in `ICompatPolicy`, the `cmdalias_t` system, and the `base_cmd.c` unified **sorted** autocomplete table (*"Inspired by Doom III"*) — with an as-built mapping flagging the still-unbuilt pieces (`cmd_scripting` `$`-substitution, `cl_filterstuffcmd` filter, `exec`/`stuffcmds`, `Cvar_WriteVariables`, the autocomplete tree) and the observer `old_value` drift (as-built refresh, 2026-07-06)
- [deep-dive-host.md](deep-dive-host.md) — legacy origins of `xash3dpp/host`: the `host.c` boot/`Host_Frame`/shutdown lifecycle and its fixed per-frame call order (Q-1), the twin `host_status_t` (process) vs `host_state_t` (map-load FSM, `host_state.c`) state machines, the `Host_Error`/`Sys_Error` + `g_abortframe`/`return_from_main_buf` `setjmp`/`longjmp` fatal model (replaced by the OQ-1 flag-poll `signal_frame_abort` + recursive-abort→`XASH_FATAL` guard), the `-bugcomp` parse-once bitfield + timing/lifecycle cvar roster split (timing→`core::Clock`, lifecycle still `TODO`), and an as-built mapping flagging the richer `EngineContext` member set (networking+server real, Winsock bracketed by EngineContext), the D-1 accessor-in-host split, and the server-as-level-executor wiring (as-built refresh, 2026-07-06)
- [deep-dive-abi.md](deep-dive-abi.md) — legacy origins of `xash3dpp/abi`: the server/client game-DLL load-time export negotiation (`GiveFnptrsToDll` push + `GetEntityAPI`/`GetEntityAPI2`/`GetNewDLLFunctions` pull in `sv_game.c`, `INTERFACE_VERSION` 138 + frozen 159/50/5 slot counts, the Win32 PE-loader name-special-case), and the `Host_Error` `GAME_EXPORT` frozen direct-symbol contract (replaced behind the signature by `core::log_va` + the OQ-1 `signal_frame_abort` flag-poll reached through the host-owned `current_engine_context()` accessor after the D-1 relocation) — with a legacy→`xash3dpp/abi` as-built mapping and the frozen static-return-buffer thread contract (as-built refresh, 2026-07-06)
- [deep-dive-launcher.md](deep-dive-launcher.md) — legacy origins of `xash3dpp/launcher`: the `game_launch/game.cpp` bootstrap executable (`WinMain`/`main` entry, `CommandLineToArgvW`+`wcstombs` argv marshalling, the `Sys_LoadEngine` `LoadLibrary`/`dlopen` + `GetProcAddress("Host_Main")` DLL handshake, the `Sys_ChangeGame` presence-as-flag no-op, the Win32 `SDL2.dll` availability probe, Sailfish `XASH3D_BASEDIR`/`RODIR` `setenv`, and the `NvOptimusEnablement`/`AmdPowerXpressRequestHighPerformance` GPU-select `.exe` exports) plus the `android/` `XashActivity`/`getLibraries` JNI bootstrap peer — with a legacy→`xash3dpp/launcher` as-built mapping showing the dissolved DLL boundary (static link to `xash3dpp_host`, typed `HostArgs` handoff, `register_thread_role(Main)` as the new first statement) (as-built refresh, 2026-07-06)
- [deep-dive-bsp-loader.md](deep-dive-bsp-loader.md) — BSP v29/v30/BSP2 on-disk format, `mod_bmodel.c` load path, hull construction, map CRC, 18-item quirk list (Chunk 5 recon, 2026-07-04; +as-built `disk_format.hpp`/`WorldData` cross-ref, 2026-07-06)
- [deep-dive-trace-pvs.md](deep-dive-trace-pvs.md) — `pm_trace.c` hull-trace kernel (exact pseudocode + epsilons), contents/PVS query surface, map_loader-vs-server split, fixture guidance (Chunk 5 recon, 2026-07-04; +as-built cross-ref and a new §7 PHS build path `Mod_CalcPHS`/`Mod_CompressPVS` per Q-19, 2026-07-06)
- [deep-dive-delta-encoder.md](deep-dive-delta-encoder.md) — `net_encode.c` delta tables, DT_* flags + wire widths, delta.lst grammar, field-codec math, Xash vs GoldSrc wire dialects, Delta_AddEncoder, baselines (retroactive networking recon, reconstructed 2026-07-04; primary input for the Chunk 6 server)
- [deep-dive-networking.md](deep-dive-networking.md) — legacy networking **core** (the parts the delta dive omits): `net_ws.c` UDP socket lifecycle + the three-function socket seam + loopback ring + split/reassembly + fakelag + async DNS, `net_chan.c` reliability (single-buffer resend) + fragmentation + flow control + munge/compression flags, `net_buffer.c` `sizebuf_t` bit/byte codec + quantised coords/angles + `iAlternateSign`, the OOB / `-1`/`-2`/`-3` packet-header magic, `common.c` LZSS (`LZSS` magic + 4096 window), and `masterlist.c` heartbeat/query — with a legacy→`xash3dpp/networking` as-built mapping (`NetworkContext` pimpl, injected `IPlatformSockets`, `net_from` eliminated, `NetAddress`/`MessageBuf` value types, `IProtocolDriver` per-channel dialect select) (as-built refresh, 2026-07-06)
- [deep-dive-server-lifecycle.md](deep-dive-server-lifecycle.md) — `sv_main/sv_init/sv_cmds`: spawn/activate/deactivate ordering, Host_ServerFrame loop, cvar roster, console commands (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-game-dll-bridge.md](deep-dive-server-game-dll-bridge.md) — `sv_game.c` + eiface/edict/progdefs: enginefuncs_t catalogue, DLL load negotiation, edict lifecycle, string pool, user messages, 32-item bug-compat list (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-clients.md](deep-dive-server-clients.md) — `sv_client/sv_custom/sv_query/sv_filter/sv_log`: connection state machine, usercmd parsing, resource/consistency/download system, queries/rcon/bans/logging (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-physics.md](deep-dive-server-physics.md) — `sv_phys/sv_move/sv_pmove`: MOVETYPE dispatch, pushers, monster locomotion, pmove bridge + unlag, physics-interface hooks (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-world-frame.md](deep-dive-server-world-frame.md) — `sv_world/sv_frame`: areanodes, trace composition, PVS/PHS multicast, snapshot/delta pipeline (Chunk 6 recon, 2026-07-04)
- [deep-dive-server-save-boundary.md](deep-dive-server-save-boundary.md) — `sv_save.c` boundary with the server core: changelevel ordering, game-DLL save callbacks, SAVERESTOREDATA ABI, separation verdict input (Chunk 6 recon, 2026-07-04; format internals deferred to Chunk 8)
- [deep-dive-content.md](deep-dive-content.md) — legacy content pipeline (Chunk 7): the `model.c` `mod_known[]` handle cache + slot-0-world + 4-state `needload` FSM + level-transition purge + CRC cheat-detect, the frozen studio v10 format (`studiohdr_t`/`mstudio*` strides + bone-controller flags), the bit-exact studio bone/pose/hull math (`R_StudioCalcBones` merged RLE decompressor, `Mod_GetBonePosition`/`…GetAttachment`/`Mod_HullForStudio`, the swappable `pBlendAPI` solver), the sprite (`IDSP`) + parse-minimal alias (`IDPO`) loaders, and the `imagelib` byte-exact codec family (`imglib_t image` global, palette Q1-vs-HL classification, texgamma/luma/index-255, the texture-name-prefix quirk catalogue, DDS/KTX2-kept-compressed, NeuQuant) — with a legacy→`xash3dpp/content` + `xash3dpp_imagelib` as-built mapping (`ModelCache` pimpl + opaque `ModelHandle`, `StudioView` typed sub-views, stateless `IImageCodec` codecs, `constexpr` palettes, injected `IModelPostProcess`/`IBoneSolver`) and the Q-18 bone-math no-touch flag (as-built refresh, 2026-07-06)

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
