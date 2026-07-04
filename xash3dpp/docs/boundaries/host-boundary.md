# Host Boundary Spec

> **Chunk numbering note (2026-07-04):** this spec predates the map_loader
> renumbering; its chunk references have been updated to the authoritative
> `implementation-plan.md` scheme (5 = map_loader, 6 = server, 7 = content,
> 12 = client, 13 = renderer). Roadmap-position statements remain a Chunk-3
> snapshot otherwise.
>
> Legacy sources surveyed:
> `engine/common/host.c` (~1370 lines), `engine/common/host_state.c` (~240 lines),
> `engine/common/common.h` (host structs), `game_launch/game.cpp` (legacy launcher),
> `Documentation/codex/legacy/engine/common-audit.md`.
>
> **Skeleton already in place** — do not re-derive:
>
> - [xash3dpp/include/xash3dpp/host/host.hpp](../../include/xash3dpp/host/host.hpp) — `Host` pimpl with `HostArgs`, `HostStatus`, `Main / init / RunFrame / RequestShutdown`
> - [xash3dpp/include/xash3dpp/host/engine_context.hpp](../../include/xash3dpp/host/engine_context.hpp) — `EngineContext` flat owner struct
> - `decisions-architecture.md` Q-1 / Q-2 / Q-4 / Q-6 are locked (subsystem ownership model, `EngineContext` shape, init-params style, threading model)

## Responsibility

The host module is the engine's lifecycle coordinator. It does five things and
nothing else:

1. **Bootstrap** — receives the parsed `HostArgs` from the launcher, creates
   the host memory pool, and runs subsystem `init()` calls in dependency
   order via `EngineContext::init`.
2. **Frame loop** — calls `Clock::tick()` once per `RunFrame()` and invokes
   the per-frame entry points on input, server, client, HTTP, and platform
   event pumps in a fixed order. The cvar-driven frame-rate, throttle, and
   sleep policy live in [`core::Clock`](../architecture/core/) — see OQ-3.
3. **Map-load coordination** — drives `MapLoader::run_frame_step()` from
   `RunFrame`. The state machine itself (legacy `host_state_t` / `game_status_t`)
   moves out of host into the `MapLoader` subsystem — see OQ-2.
4. **Error containment** — `Host::signal_frame_abort()` records the abort
   reason and short-circuits the rest of the tick; the next `RunFrame()` runs
   the recovery cleanup. No `setjmp`/`longjmp`. See OQ-1.
5. **Game-change orchestration** — receives the `game <dir>` console command
   and exposes it through a host method; the underlying restart-vs-hot-reload
   mechanism is deferred (OQ-8).

It does **not** parse map files, ship packets, allocate game entities, talk
to the renderer, own the frame clock, or own the map-load FSM. Subsystem-owned
fields formerly attached to `host_parm_t` are redistributed per the
*Owned state* table below.

## External ABI contracts

The host exports three C entry points consumed by the launcher DLL. The
launcher in `game_launch/game.cpp` is a thin loader that
`dlsym`s/`GetProcAddress`s these three names and calls `Host_Main`.

| Symbol | Signature | Purpose |
|---|---|---|
| `Host_Main` | `int (int argc, char **argv, const char *progname, int bChangeGame, pfnChangeGame pChangeGame)` | Full engine lifecycle in one call; blocks until shutdown. |
| `Host_Shutdown` | `void (void)` | Last-resort cleanup; the launcher calls this only if `Host_Main` returns. |
| `pfnChangeGame` | `void (*)(const char *progname)` | Callback passed *in* by the launcher; present means "engine is allowed to enable the `game` command". The real implementation `Sys_ChangeGame` in the legacy launcher is a no-op — the engine restarts via `Sys_NewInstance` instead. |

These three names are an internal contract between the launcher and the
engine binary; they are not part of the GoldSrc SDK ABI. The rewrite is free
to redesign them — but the launcher must be updated in lockstep. The
xash3dpp launcher will not be a separate DLL boundary; the rewrite folds the
launcher into the same executable, so this ABI dissolves entirely.

In addition, `Host_Error` is a `GAME_EXPORT` direct symbol export consumed
by the game DLL's import table (it is **not** routed through `enginefuncs_t`;
the vtable-routed sibling is `pfnSys_Error`). This is a real ABI surface that
must be preserved — see OQ-10 for the adapter design.

| Symbol | Surface | Notes |
|---|---|---|
| `Host_Error(const char *fmt, ...)` | `engine/eiface.h` (`pfnHostError`) | Game DLLs call this for fatal map-load errors. The rewrite must keep the variadic C signature in the adapter layer; internally it can route to a typed error path. |

## Interface (what the rest of the engine calls)

In the rewrite, every internal caller goes through `Host` or `EngineContext`
member functions, not through global free functions. The table below maps
legacy entry points to the new method.

### Lifecycle

| Legacy | New |
|---|---|
| `Host_Main(argc, argv, progname, bChangeGame, pChangeGame)` | `Host::Main(const HostArgs&)` |
| `Host_InitCommon(...)` | `Host::init(const HostArgs&)` |
| `Host_FreeCommon()` + `Host_Shutdown()` | `Host::~Host()` via `EngineContext` destructor |
| `Host_ExitInMain()` | `Host::RequestShutdown(const char *reason)` |
| `Host_ShutdownWithReason(reason)` | Same as above |

### Frame loop and timing

All timing state and policy lives in `core::Clock` (see OQ-3). Host only
orchestrates the per-frame call order.

| Legacy | New |
|---|---|
| `Host_Frame(time)` | `Host::RunFrame()` (no parameter — `Clock` owns the wall clock) |
| `Host_FilterTime(time)` | `core::Clock::tick()` returns `false` if the frame budget is not yet reached |
| `Host_CalcFPS()` | `core::Clock` private |
| `Host_CalcSleep()` | `core::Clock` private (reads `sv_hibernate_*` via observer) |
| `Host_Autosleep(dt, scale)` | `core::Clock` private |
| `host.realtime`, `host.frametime`, `host.realframetime`, `host.framecount`, `host.starttime`, `host.pureframetime` | `core::Clock` accessors; `Host::realtime()` forwards to `engine_ctx.clock.realtime()` for source compatibility |

### Map-load state machine

The legacy `host_state_t` / `game_status_t` FSM moves out of host into a
new `MapLoader` subsystem owned by `EngineContext` (see OQ-2).

| Legacy | New |
|---|---|
| `COM_InitHostState()` | `MapLoader` constructor / `init()` |
| `COM_NewGame(map)` | `MapLoader::new_game(std::string_view map)` |
| `COM_LoadLevel(map, bg)` | `MapLoader::load_level(std::string_view map, bool background)` |
| `COM_LoadGame(map)` | `MapLoader::load_game(std::string_view map)` |
| `COM_ChangeLevel(map, landmark, bg)` | `MapLoader::change_level(...)` |
| `COM_Frame(time)` outer loop | `MapLoader::run_frame_step()` called by `Host::RunFrame()` |
| `Host_RunFrame(time)`, `Host_ShutdownGame()`, `Host_SetState(...)`, `Host_SetNextState(...)` | `MapLoader` private |
| `Host_AbortCurrentFrame()` | replaced by `Host::signal_frame_abort()` — see *Frame abort propagation* under Quirks |

Client and server subscribe as `IMapLoaderObserver` for `on_load_begin` /
`on_load_end` callbacks (loading-plaque, demo bookkeeping).

### Error handling

| Legacy | New |
|---|---|
| `Host_Error(fmt, ...)` (engine-internal callers) | `Host::signal_frame_abort(core::ErrorCode code, std::string_view detail)` |
| `Host_Error(fmt, ...)` (game DLL via `GAME_EXPORT`) | C ABI shim in `xash3dpp/src/abi/engine_funcs.cpp` — see OQ-10 |
| `Host_EndGame(abort, fmt, ...)` | `Host::end_game(...)` |
| `Host_ValidateEngineFeatures(mask, features)` | `Host::validate_engine_features(...)` |

### Console commands registered by host

| Command | Legacy `_f` | Notes |
|---|---|---|
| `quit` / `exit` | `Sys_Quit_f` | dedicated only |
| `host_writeconfig` | `Host_WriteConfig` | non-dedicated only |
| `minimize` | `Platform_Minimize_f` | non-dedicated only |
| `memlist` | `Mem_Stats_f` | always |
| `game <dir>` | `Host_ChangeGame_f` | only if launcher passed a non-null `pChangeGame` |
| `host_error`, `sys_error`, `crash` | dev-extended only | dev/test |

All of these get registered through `CmdCvarContext` at `Host::init` time.

## Dependencies

The host sits at the **top** of the static dependency graph: it is the only
subsystem that calls every other subsystem's `init`/`shutdown`/per-frame entry
point. Layered downward:

| Dependency | Why host needs it |
|---|---|
| `memory` (global, exempt from `EngineContext`) | `create_pool("host")` is the first allocation; held as `Host::Impl::pool_` |
| `platform` | `platform::get_time()` (via `Clock`), `platform::console::poll_line()` (dedicated stdin — OQ-9), `platform::sleep_ms`, message-box, crash handler, `Sys_NewInstance` |
| `core` (logging + clock + thread-roles) | `core::log` / `core::logf` for boot diagnostics; `core::Clock` for frame timing; `core::assert_thread_role(ThreadRole::Main)` at every public host method |
| `filesystem` | `FS_Init`, `FS_LoadGameInfo`, `FS_FileExists` (for `valve.rc` / `config.cfg` lookup), `FS_Shutdown` |
| `cmd_cvar` | host cvars (`host_developer`, `host_gameloaded`, `host_clientloaded`, `host_limitlocal`, `con_gamemaps`, `host_allow_materials`, ...) and host commands (`memlist`, `game`, `quit`, ...); `Cbuf_AddText` / `Cbuf_Execute` for `valve.rc` / `config.cfg` / `userconfigd` / `+stuffcmds` |
| `core::Clock` *(new, Chunk 3)* | frame timing, per-frame `tick()`, owns the timing cvars (`host_maxfps`, `fps_override`, `host_framerate`, `host_sleeptime`, `host_sleeptime_debug`, `sys_timescale`, `sys_ticrate`) |
| `MapLoader` *(new, Chunk 3)* | game-state FSM; `MapLoader::run_frame_step()` called from `Host::RunFrame` |
| `networking` *(Chunk 2)* | `NET_Init`, `Netchan_Init`, `HTTP_Init`, `HTTP_Run` (per-frame), `NET_InitMasters` |
| `server` *(Chunk 6)* | `SV_Init`, `SV_ExecLoadLevel`, `SV_ExecLoadGame`, `SV_ExecChangeLevel`, `SV_ShutdownGame`, `SV_Shutdown`, `SV_Active`, `SV_GetPlayerCount`, `Host_ServerFrame` |
| `client` *(Chunk 12, non-dedicated only)* | `CL_Init`, `Host_InputFrame`, `Host_ClientBegin`, `Host_ClientFrame`, `CL_Disconnect`, `CL_Drop`, `CL_ClearEdicts`, `CL_IsPlaybackDemo`, `CL_IsRecordDemo`, `CL_IsInGame`, demo framerate, `SCR_BeginLoadingPlaque`, `SCR_CheckStartupVids`, `UI_CreditsActive`, `UI_SetActiveMenu`, `Key_SetKeyDest`, `IN_Init`, `Key_Init`, `IN_GyroCheckAvailability` |
| Content / world *(Chunks 5/7)* | `Mod_Init`, `Mod_FreeAll`, `Image_Init`, `Sound_Init`, `Image_Shutdown`, `Sound_Shutdown`, `HPAK_Init`, `HPAK_CheckIntegrity`, `HPAK_FlushHostQueue`, `Host_InitDecals` |

### `EngineContext` member order (Chunk 3 snapshot)

C++ guarantees declaration-order construction and reverse-order destruction;
the order below is therefore the init/shutdown ordering and must satisfy the
dependency arrows.

```cpp
struct EngineContext {
    Filesystem      filesystem;     // Chunk 1 (done)
    CmdCvarContext  cmd_cvar;       // Chunk 1 (done)
    core::Clock     clock;          // Chunk 3 (new)
    // NetworkContext networking;   // Chunk 2
    MapLoader       map_loader;     // Chunk 3 (new)
    Host            host;           // Chunk 3 (new)
    // ServerContext  server;       // Chunk 6
    // ClientContext  client;       // Chunk 12
    uint32_t        bugcomp = 0;    // OQ-7: parsed once from HostArgs; read by subsystems at point of behaviour divergence
};
```

## Owned state

Today this is one god-struct: `host_parm_t host` in `engine/common/host.c:42`.
The rewrite **must split this**. Below is the field-by-field audit and the
proposed new owner.

### Timing — **moves out** to `core::Clock`

All fields below are owned by `core::Clock`; host accessors that return them
forward through `engine_ctx.clock`.

| Field | Type | New owner | Notes |
|---|---|---|---|
| `realtime` | `double` | `core::Clock` | wall clock; renderer ABI reads via `ref_host_t` (OQ-4 / OQ-6) |
| `frametime` | `double` | `core::Clock` | last frame delta, clamped + `host_framerate` override applied; renderer ABI |
| `realframetime` | `double` | `core::Clock` | unclamped frame delta (UI animations) |
| `pureframetime` | `double` | `core::Clock` | time inside frame body excluding sleep |
| `framecount` | `uint` | `core::Clock` | frame counter |
| `starttime` | `double` | `core::Clock` | recorded at `Clock::init` time |
| `force_draw_version_time` | `double` | `client` | unrelated cosmetic — belongs in client |

### Error / abort — stays in `Host::Impl`

| Field | Type | Notes |
|---|---|---|
| `errorframe` | `uint` | recursive-`Host_Error` guard (Quirk Q-4) |
| *(new)* `frame_abort_pending` | `bool` | set by `signal_frame_abort()`; checked at frame top — see *Frame abort propagation* |
| *(new)* `frame_abort_code` | `core::ErrorCode` | reason for the abort |
| *(new)* `frame_abort_detail` | fixed buffer | optional message (no heap) |

### State machine — **moves out** to `MapLoader`

| Field | Type | New owner |
|---|---|---|
| `status` | `host_status_t` | `Host::Impl` (becomes `HostStatus`) |
| `game` (`game_status_t`) | embedded struct | `MapLoader::Impl` (becomes `MapLoadState` + level/landmark/flags) |
| `type` | `instance_t` | replaced by `HostArgs::dedicated` |

### Memory — stays in `Host::Impl`

| Field | New owner |
|---|---|
| `mempool` | `Host::Impl::pool_` |
| `imagepool`, `soundpool` | **move** to the respective subsystems (content, sound) |

### Command-line — replaced by `HostArgs`

| Field | New owner |
|---|---|
| `argc`, `argv` | `HostArgs::argc`, `HostArgs::argv` |
| `default_gamedir` | `HostArgs::basedir` / `HostArgs::gamedir` |
| `bugcomp` | `HostArgs::bugcomp` (`uint32_t`) → copied into `EngineContext::bugcomp` at init; subsystems read directly (OQ-7) |

### Game-DLL / DLL paths — **move** to server / client subsystems

| Field | New owner |
|---|---|
| `gamedll` | `server` |
| `clientlib` | `client` |
| `menulib` | `client` (UI subsystem) |
| `downloadfile`, `downloadcount` | `client` (or HTTP subsystem if folded) |

### Console / cheats / input flags — **move** out

| Field | New owner |
|---|---|
| `allow_console`, `allow_console_init` | `Host::Impl` (OQ-5 resolved — stays in host; core/ has no state by design) |
| `allow_cheats` | `server` |
| `key_overstrike`, `mouse_visible`, `window_center_x/y`, `hWnd` | `client` (input subsystem) |
| `deferred_cmd`, `stuffcmds_pending`, `config_executed` | `cmd_cvar` |
| `userinfo_changed`, `movevars_changed`, `renderinfo_changed` | observers in respective subsystems |
| `change_game`, `shutdown_issued`, `apply_opengl_config`, `textmode` | host (lifecycle flags — stay) |
| `rd` (`host_redirect_t`) | `Host::Impl` (paired with `allow_console`; later may move to a dedicated console subsystem) |

### Decals / hull bounds — **move** out

| Field | New owner |
|---|---|
| `draw_decals[MAX_DECALS][MAX_QPATH]` | `content` subsystem (decal registry) |
| `player_mins[MAX_MAP_HULLS]`, `player_maxs[MAX_MAP_HULLS]`, `_backup` | `physics` / `pm_shared` (these are game-side hull dims) |
| `trace_bounds_pushed` | same as above |

### Cvars owned by host (after the audit)

Host registers only the cvars that drive host-level lifecycle / policy. All
timing cvars move to `core::Clock` (OQ-3). All hibernation cvars are read
by `Clock::tick()` but registered by host because they describe server
policy.

**Registered by `Host::init()`:**

- Lifecycle / mode: `host_developer`, `host_serverstate`, `host_gameloaded`,
  `host_clientloaded`, `host_allow_materials`, `host_limitlocal`,
  `con_gamemaps`, `cl_background`, `sv_background`
- Dedicated-server hibernation: `sv_hibernate_when_empty`,
  `sv_hibernate_when_empty_sleep`, `sv_hibernate_when_empty_include_bots`
- Read-only info (`Cvar_Getf` at boot): `buildnum`, `ver`, `host_ver`,
  `host_lowmemorymode`, `host_hl25_extended_structs`, `host_allow_changegame`

**Registered by `core::Clock::init()` (moved out of host):**

- Frame timing: `host_maxfps`, `fps_override`, `host_framerate`,
  `host_sleeptime`, `host_sleeptime_debug`, `sys_timescale`, `sys_ticrate`

## Quirks and invariants

### Q-1 — Per-frame call order is fixed (legacy `Host_Frame`)

```text
Platform_RunEvents()            // (non-dedicated only, via Host_RunFrame)
Host_FilterTime(time)           // bail if frame-budget not reached
Host_InputFrame()               // poll input -> commands
Host_ClientBegin()              // demo / paused / overrides
Host_GetCommands()              // dedicated console stdin -> Cbuf
Host_ServerFrame()              // server tick: physics, AI, write packets
Host_ClientFrame()              // client tick: prediction, render, sound
HTTP_Run()                      // poll HTTP downloads
```

Reordering will break GoldSrc demos and protocol timing. `Host::RunFrame()`
must call these in this exact order.

### Q-2 — `host_status_t` is **not** `host_state_t`

Two separate state machines, both tracked in `host_parm_t`:

- `host_status_t host.status` — process-level: `HOST_INIT`, `HOST_FRAME`,
  `HOST_SHUTDOWN`, `HOST_ERR_FATAL`, `HOST_SLEEP`, `HOST_NOFOCUS`,
  `HOST_CRASHED`. Drives `Host_CalcSleep` and the `Host_Main` while-loop.
- `host_state_t GameState->curstate / nextstate` — map-load FSM:
  `STATE_RUNFRAME`, `STATE_LOAD_LEVEL`, `STATE_LOAD_GAME`, `STATE_CHANGELEVEL`,
  `STATE_GAME_SHUTDOWN`. Drives `COM_Frame`.

Current `HostStatus` (`kInit / kRunning / kSleep / kShutdown`) maps to the
first one. The second one needs a separate `MapLoadState` enum.

### Q-3 — Frame abort propagation (no `setjmp`/`longjmp`)

Legacy `Host_Error` uses `setjmp`/`longjmp` (`return_from_main_buf` in
`Host_Main`, `g_abortframe` per frame). This pattern is incompatible with
C++ RAII — stack-jumping over destructors is undefined.

**Resolved (OQ-1)** — hybrid mechanism, no exceptions, no `longjmp`:

1. **Engine-internal callers** propagate failure via
   `std::expected<void, core::ErrorCode>` (Chunk 2 vocabulary, per
   decisions-architecture.md §3 Q-5). Each per-frame entry point
   (`MapLoader::run_frame_step`, `Server::frame`, `Client::frame`,
   `Networking::http_run`) returns `expected<void, ErrorCode>`;
   `Host::RunFrame` short-circuits on first error.
2. **Game-DLL callers** invoke the C ABI symbol `Host_Error` (see Q-14 /
   OQ-10). The adapter formats the variadic args, logs at `Fatal`, and
   calls `Host::signal_frame_abort(ErrorCode::HostFatal, detail)` which
   sets `Impl::frame_abort_pending = true` + records `frame_abort_code`
   and `frame_abort_detail`.
3. **Recovery** runs in one place at the top of the next `RunFrame`:
   while the flag is set, host performs the legacy cleanup
   (`SV_Shutdown`, `CL_Drop`, `CL_ClearEdicts`, `Mod_FreeAll`) and clears
   the flag. All destructors run normally.

The legacy "kill the frame mid-tick" semantic is preserved: once the flag
is set, every subsequent per-frame entry point sees it and bails early
via `if (host.frame_abort_pending) return std::unexpected(...)`.

### Q-4 — `Host_Error` recursion guard

`recursive` static + `host.errorframe == host.framecount` check means a
second `Host_Error` in the same frame escalates to `Sys_Error` (fatal).
Must preserve this — game DLLs sometimes throw two errors back-to-back
during map-load failure and the engine relies on the second one being fatal.

### Q-5 — Three-init-phase ordering

Legacy `Host_Main` runs:

1. `Host_InitCommon` — memory, platform, FS, image, sound, FS gameinfo, decals
2. `Mod_Init`, `NET_Init`, `Netchan_Init` — networking
3. `SV_Init`, `CL_Init` — game-DLL load (server and client)
4. `HTTP_Init`, `SoundList_Init`
5. `valve.rc` exec → `config.cfg` exec → `userconfigd` exec → `+stuffcmds` exec

The startup script execution **must run after** `SV_Init`/`CL_Init` because
`valve.rc` references cvars that the game DLLs register. `EngineContext::init`
must respect this ordering.

### Q-6 — Dedicated-server `host.status` flip

For dedicated builds the input subsystem is missing, so `host.status` never
naturally transitions from `HOST_INIT` to `HOST_FRAME`. `Host_Main` forces
`host.status = HOST_FRAME` right before the main loop in dedicated mode.

### Q-7 — `host_framerate` only applies in singleplayer + non-demo

`Host_FilterTime`:

```c
if( host_framerate.value > 0.0f && Host_IsSinglePlayerGame() &&
    !CL_IsPlaybackDemo() && !CL_IsRecordDemo( ))
    host.frametime = bound( MIN_FRAMETIME, host_framerate.value * scale, MAX_FRAMETIME );
```

This is a debugging knob; preserve the gate exactly.

### Q-8 — GoldSrc compatibility frame cap

`Host_CalcFPS` returns a hardcoded `31.0` when:

```c
!SV_Active() && CL_Protocol() == PROTO_GOLDSRC &&
cls.state != ca_disconnected && cls.state < ca_validate
```

This is the GoldSrc connect-handshake rate-limit; required for joining HL1
servers without getting kicked.

### Q-9 — Dedicated-server hibernation

When `sv_hibernate_when_empty` is on and player count is zero,
`Host_CalcSleep` returns `sv_hibernate_when_empty_sleep` (default 500 ms)
instead of `host_sleeptime` (default 1 ms). Major idle-CPU saving.

### Q-10 — `Host_Frame` first frame logs "time to first frame"

`if( host.framecount == 0 ) Con_DPrintf("Time to first frame: %.3f")` — a
boot-perf metric that should remain accessible (rename to a host stat).

### Q-11 — `host.bugcomp` is a bitfield set once at boot

Parsed from `-bugcomp peoei,gsmrf,sp_attn_none,get_game_dir_full` and never
mutated afterwards. Becomes `HostArgs::bugcomp` (`uint32_t`), copied into
`EngineContext::bugcomp` at init. Per OQ-7 each `BUGCOMP_*` bit is consumed
in exactly one subsystem at the point of behaviour divergence; host owns
the parse, not the dispatch.

### Q-12 — `Host_ChangeGame_f` restarts the process

The `game <dir>` console command does not hot-swap modules; it calls
`Sys_NewInstance` which `exec`s the engine binary with new arguments. The
rewrite is free to redesign this, but the user-facing command name and
semantics must remain.

### Q-13 — `Sys_NewInstance` requires `pChangeGame != nullptr`

The launcher must pass a non-null `pChangeGame` callback for the `game`
command to be enabled. The legacy `Sys_ChangeGame` is a no-op stub — the
*presence* of the symbol is the flag. The new launcher should pass a
`changegame_enabled` bool in `HostArgs` instead.

### Q-14 — Game DLL `Host_Error` ABI

`Host_Error` is a `GAME_EXPORT`-tagged **direct symbol** exported from the
engine binary (not a `enginefuncs_t` vtable slot). Game DLLs call it via
their import table; the sibling vtable-routed path is `pfnSys_Error` in
eiface.h. The variadic C signature
`void Host_Error(const char *fmt, ...)` must survive.

The shim lives in `xash3dpp/src/abi/engine_funcs.cpp` (see OQ-10) and
calls `Host::signal_frame_abort(...)` after formatting + logging at
`Fatal`. It reaches the live host through
`xash::abi::current_engine_context()` — the single documented exception
to the "no global accessor" rule (decisions-architecture.md §3 Q-2),
justified because `extern "C"` ABI symbols have no other way to reach
the context.

## What the launcher (`game_launch/game.cpp`) does today, and where each part goes

| Launcher responsibility | New home |
|---|---|
| `WinMain` / `main` entry, argv unicode conversion | xash3dpp_launcher (already exists in `xash3dpp/src/launcher/`) |
| `LoadLibrary("xash.dll")` + `GetProcAddress("Host_Main")` | **removed** — host is statically linked into the launcher executable |
| `LoadLibrary("SDL2.dll")` (Windows availability check) | platform — `platform::sdl::ensure_runtime()` (or build-time DLL search path) |
| `Sys_ChangeGame` no-op stub | **removed** — replaced by `HostArgs::changegame_enabled = true` |
| `XASH_GAMEDIR` default `"valve"` | `HostArgs::basedir` default |
| NVIDIA Optimus / AMD PowerXpress export symbols | launcher executable (must stay as exported symbols of the .exe) |
| Sailfish `$HOME/xash` `XASH3D_BASEDIR` setup | launcher / platform |

The xash3dpp launcher already has a `HostArgs` shape and a thin `argv → HostArgs` parser. The contract is:
**launcher = arg parsing + envvar resolution + Optimus exports**;
**host = everything else.**

## Resolved decisions

All ten open questions from the initial draft were resolved during the
follow-up design session. Outcomes:

| # | Decision | Lands in |
|---|---|---|
| OQ-1 | **Hybrid abort.** Engine-internal callers use `std::expected<void, core::ErrorCode>`; game-DLL `Host_Error` symbol sets `Host::Impl::frame_abort_pending` via `signal_frame_abort`. Recovery runs once at the top of the next `RunFrame`. No `setjmp`/`longjmp`, no exceptions. | Quirk Q-3 |
| OQ-2 | **New `MapLoader` subsystem** owned by `EngineContext` between `cmd_cvar`/`Clock` and server/client. Host calls `map_loader.run_frame_step()` from `RunFrame`. Client/server attach as `IMapLoaderObserver` (`on_load_begin` / `on_load_end`). | Interface § Map-load state machine; Dependencies table |
| OQ-3 | **New `core::Clock`** pimpl class owns `realtime / frametime / realframetime / framecount / starttime / pureframetime` and the timing cvars (`host_maxfps`, `fps_override`, `host_framerate`, `host_sleeptime`, `host_sleeptime_debug`, `sys_timescale`, `sys_ticrate`). `Host::realtime()` forwards to `engine_ctx.clock.realtime()`. Atomics on the doubles for renderer-thread reads. | Interface § Frame loop and timing; Owned state § Timing; Cvars owned |
| OQ-4 | **`host_parm_t` ABI** — confirmed only the renderer reads `host_parm_t` fields directly (via `ref_host_t` / `PARM_GET_HOST_PTR`, fields `realtime / frametime / features`). Game/client DLLs go through `enginefuncs_t` exclusively. No further audit needed. | (resolved) |
| OQ-5 | **`allow_console` stays in host.** `core/` is namespace-only-no-state by design; promoting one bool to a stateful `CoreContext` is over-decomposition. `key_overstrike` and `rd` (`host_redirect_t`) likewise stay in host until a dedicated console subsystem lands. | Owned state § Console / cheats / input flags |
| OQ-6 | **`ref_host_t` preserved exactly** until Chunk 13. Renderer-ABI shim (Chunk 13 stub) holds a `static ref_host_t s_ref_host;` updated once per frame from `core::Clock` + `EngineContext::features`. Renderer plugin reads pointer once during init. New fields, if any, go to a v2 struct accessed via a new `PARM_GET_HOST_PTR_V2`. | (resolved) |
| OQ-7 | **Centralised parse, distributed consumption.** `HostArgs::bugcomp` (`uint32_t`) parsed by launcher; copied to `EngineContext::bugcomp` at init. Each `BUGCOMP_*` bit is consumed in exactly one subsystem at the point of behaviour divergence. No `host_bugcomp` cvar. | Quirk Q-11; `EngineContext` member-order block |
| OQ-8 | **Deferred.** `Sys_NewInstance` exec-style restart vs hot-reload mechanism is unspecified. Constraint: user-facing `game <dir>` command must continue to work. Re-evaluate after server / client chunks complete. | (deferred) |
| OQ-9 | **`platform::console::poll_line() -> std::optional<std::string_view>`** is added to platform. `Host::Impl::poll_dedicated_input()` is a 5-line wrapper that forwards each line to `cmd_cvar::cbuf_add_text` + `cbuf_execute`. Stays on the main thread; no dedicated stdin worker pending profiling evidence. | Interface § Console commands; Dependencies § platform |
| OQ-10 | **`xash3dpp/src/abi/engine_funcs.cpp`** houses `extern "C"` direct-symbol exports the game DLL needs (`Host_Error`, ...). Reaches the live context via `xash::abi::current_engine_context()` — documented exception to Q-2 "no global accessor," justified because game DLLs cross a C ABI boundary. Same file will host every other `GAME_EXPORT` symbol discovered during the server/client chunks. | Quirk Q-14 |
| OQ-11 | **`Clock::set_frame_rate_gate(bool(*fn)()noexcept)`** post-init setter. When set, `host_framerate` override in `Clock::tick()` activates only when `fn()` returns true (i.e., a single-player map is running). Until Chunk 6 `Server::init()` wires the real predicate, the gate is `nullptr` = always false — `host_framerate` has no effect in Chunk 3. | `core/clock.hpp`; `clock.cpp` tick(); Chunk 6 Server::init() |

---

## Fixed Limits (`xash3dpp` rewrite)

All compile-time capacity limits are defined in `xash3dpp/include/xash3dpp/limits.hpp` under the `// host subsystem` block and are override-able at build time.

| Constant | Default | Override macro | Notes |
|---|---|---|---|
| `::xash::limits::host_frame_abort_detail_buf` | `256` | `XASH_LIMIT_HOST_FRAME_ABORT_DETAIL_BUF` | Buffer size (bytes, incl. null terminator) for the frame-abort detail string stored in `Host::Impl::frame_abort_detail`. |

---

## Observability / Stats

`HostStats` (declared in `xash3dpp/include/xash3dpp/host/host.hpp`) is an always-on snapshot struct returned by reference from `Host::stats() const noexcept`.

| Field | Type | Description |
|---|---|---|
| `status` | `HostStatus` | Current lifecycle state (`kInit`, `kRunning`, `kSleep`, `kShutdown`). |

`Host::Impl::stats_` is kept in sync at every point where `Impl::status` is written:

| Write site | Location |
|---|---|
| `Impl::shutdown()` — `kShutdown` | `host.cpp` |
| `init()` — `kInit` | `host.cpp` |
| `init()` — `kRunning` | `host.cpp` |
| `Host::RequestShutdown()` — `kShutdown` | `host.cpp` |
