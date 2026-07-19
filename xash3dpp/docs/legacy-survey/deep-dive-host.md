# Deep Dive: Legacy Origins of `xash3dpp/host` — Frame Loop, State Machines, Feature Flags, and the `longjmp` Error Model

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy behaviours the `xash3dpp`
**host** subsystem coordinates — the `engine/common/host.c` boot/frame/shutdown
lifecycle, the twin `host_status_t` / `host_state_t` state machines
(`host.c` + `engine/common/host_state.c`), the `-bugcomp` feature bitfield, and
the `Host_Error`/`Sys_Error` `setjmp`/`longjmp` fatal model. Line numbers are
against the working tree on that date; behaviour references, not design
constraints. Everything about the legacy is **reference-only**.*

Host is the engine's orchestration layer, so — like `core` — it is a
*distillation*, not a 1:1 port: the timing math moved to `core::Clock`
(see `deep-dive-core.md`), the map-load FSM moved to `map_loader`, and what
remains in `xash3dpp/host` is the boot sequencer, the frame-call-order driver,
the error-containment flag, and the `EngineContext` owner. The as-built mapping
(§6) is explicit about which legacy piece landed where.

Primary legacy sources:

- `engine/common/host.c` (~1370 lines) — `host_parm_t host` god-struct,
  `Host_InitCommon` / `Host_Main` / `Host_Frame` / `Host_Shutdown`,
  `Host_Error`, the timing cvar roster, `return_from_main_buf` jmp_buf
- `engine/common/host_state.c` (~240 lines) — the `game_status_t` map-load FSM
  (`COM_InitHostState`, `Host_SetState`/`Host_SetNextState`, `COM_Frame`,
  `Host_AbortCurrentFrame` + `g_abortframe`)
- `engine/common/common.h` — `host_parm_t`, `host_status_t`, `host_state_t`
- `game_launch/game.cpp` — the legacy launcher DLL and the `pChangeGame` flag

**Global assumptions (legacy):** all host state is **process-global**
(`host_parm_t host;` at `host.c:42`) with no context object; two `jmp_buf`s
(`return_from_main_buf` for clean exit, `g_abortframe` per-frame) provide the
error control flow; a single main thread runs everything (sound mixer / HTTP
helper threads never touch the host struct under contract).

______________________________________________________________________

## 1. The lifecycle — `Host_Main` / `Host_InitCommon` / `Host_Shutdown`

`Host_Main` (`host.c:~1140`) is the whole engine lifetime in one call. Its
skeleton:

1. `host.status = HOST_INIT` (`host.c:1011`), zero `host.errorframe`
   (`host.c:1242`).
2. `Host_InitCommon` — memory pools, platform, filesystem + gameinfo, image,
   sound, decals (the "phase 1" bring-up).
3. `Mod_Init`, `NET_Init`, `Netchan_Init` — model + networking.
4. `SV_Init`, `CL_Init` — game-DLL load (server always; client non-dedicated).
5. `HTTP_Init`, sound-list init.
6. Startup-script exec chain: `valve.rc` → `config.cfg` → `userconfigd` →
   `+stuffcmds` — **must** run after `SV_Init`/`CL_Init` because the scripts
   reference cvars the game DLLs register (rewrite Quirk Q-5).
7. `setjmp( return_from_main_buf )` (`host.c:1160`) — the clean-exit trampoline;
   `Host_ExitInMain` (`host.c:57`) `longjmp`s here to break out of the main loop
   from arbitrary depth.
8. Dedicated-only: force `host.status = HOST_FRAME` (`host.c:1296`) because with
   no input subsystem the status never naturally leaves `HOST_INIT`
   (rewrite Quirk Q-6).
9. `while(1) Host_Frame(...)` until shutdown; then `host.status = HOST_SHUTDOWN`
   (`host.c:1349`) → `Host_Shutdown`.

**`xash3dpp` mapping.** The phase ordering is preserved by
`EngineContext::init`'s declaration-order construction + explicit init sequence
(`filesystem → cmd_cvar → clock → networking → map_loader → host → server`), and
the clean-exit `longjmp` trampoline is simply **gone**: `Host::Main` runs
`while (status != Shutdown) RunFrame();` and returns normally. `Host_ExitInMain`
becomes `RequestShutdown` (sets `HostStatus::Shutdown`; the loop exits on the
next test).

______________________________________________________________________

## 2. The frame loop — `Host_Frame` fixed call order

`Host_Frame` (`host.c:658`) is short and its order is load-bearing:

```c
if( !Host_FilterTime( time )) return;          // frame-budget gate (Clock)
t1 = Platform_DoubleTime();
if( host.framecount == 0 )
    Con_DPrintf( "Time to first frame: %.3f seconds\n", t1 - host.starttime );
Host_InputFrame ();  // input -> commands
Host_ClientBegin (); // demo / paused / overrides
Host_GetCommands (); // dedicated stdin -> Cbuf
Host_ServerFrame (); // physics, AI, write packets
Host_ClientFrame (); // prediction, render, sound
HTTP_Run();          // poll downloads
host.framecount++;
host.pureframetime = Platform_DoubleTime() - t1;
```

Reordering breaks GoldSrc demos and protocol timing (rewrite Quirk Q-1). Note
`Host_Frame` is wrapped by `Host_RunFrame` (`host_state.c:150`) which calls
`Platform_RunEvents()` first (non-dedicated) and then drives the map-load FSM
transition afterwards — so the *outer* per-frame unit is
`COM_Frame` → `Host_RunFrame` → `Host_Frame`.

**`xash3dpp` mapping.** `Host::RunFrame` is the merged `Host_RunFrame`+`Host_Frame`
driver; the budget gate + timing math live in `core::Clock::tick()` (see
`deep-dive-core.md` §1). As-built, `RunFrame` wires only the pieces whose
subsystems exist: frame-abort recovery (top), `cmd_cvar->cbuf_execute()` (the
`Host_GetCommands`/Cbuf sibling), then `map_loader->run_frame_step()`. Input,
server frame, client frame, HTTP, dedicated stdin, and the "time to first frame"
metric are `TODO Chunk 6/12`. The **cbuf-before-map-step** ordering is preserved
deliberately (a `map` command issued this frame must take effect in the same
frame's FSM step).

______________________________________________________________________

## 3. Two state machines — `host_status_t` vs `host_state_t`

The legacy engine tracks **two** distinct FSMs, both on `host_parm_t`
(rewrite Quirk Q-2):

### 3.1 `host_status_t host.status` — process lifecycle

Values: `HOST_INIT`, `HOST_FRAME`, `HOST_SHUTDOWN`, `HOST_ERR_FATAL`,
`HOST_SLEEP`, `HOST_NOFOCUS`, `HOST_CRASHED`. Drives `Host_CalcSleep` (how long
to sleep between frames) and the `Host_Main` while-loop.

### 3.2 `host_state_t GameState->curstate/nextstate` — map-load FSM

`host_state.c` owns `game_status_t GameState` with a current/next state pair:
`STATE_RUNFRAME`, `STATE_LOAD_LEVEL`, `STATE_LOAD_GAME`, `STATE_CHANGELEVEL`,
`STATE_GAME_SHUTDOWN`. Transitions are guarded — every `COM_*` entry
(`COM_NewGame`, `COM_LoadLevel`, `COM_LoadGame`, `COM_ChangeLevel`) early-returns
unless `nextstate == STATE_RUNFRAME`, then calls `Host_SetNextState(...)`.
`COM_Frame` (`host_state.c:192`) is a `setjmp(g_abortframe)` loop that executes
the current state (`SV_ExecLoadLevel` / `SV_ExecLoadGame` / `SV_ExecChangeLevel`
/ `Host_ShutdownGame`) and transitions, with `SCR_BeginLoadingPlaque` fired on
load/changelevel transitions.

**`xash3dpp` mapping.** `host.status` becomes the four-value
`HostStatus{ Init, Running, Sleep, Shutdown }` in `host.hpp` (the fatal/crashed
values fold into the abort path + platform crash handler). The entire
`host_state_t` FSM **moves out** to `map_loader` (`MapLoader::run_frame_step`,
`new_game`/`load_level`/`load_game`/`change_level`; observers replace the inline
`SCR_BeginLoadingPlaque`). Host retains only the driver call
(`map_loader->run_frame_step()`) and, in `EngineContext`, wires the server as the
map-load `level_executor` (`map_loader.set_level_executor(&server)` — the
`SV_ExecLoadLevel` seam).

______________________________________________________________________

## 4. The error model — `Host_Error` / `Sys_Error` + `longjmp`

`Host_Error` (`host.c:687`, `GAME_EXPORT`) is the fatal map-load/runtime error
path game DLLs and the engine share. Its logic (`host.c:687–748`):

- Format into `static char hosterror1[MAX_SYSPATH]`.
- `host.framecount < 3` → `Sys_Error("…Init: …")` (fatal — too early to
  recover).
- `host.framecount == host.errorframe` → `Sys_Error("…Multi: …")` (second error
  **this** frame — fatal).
- Otherwise print `S_RED` message (dev → open console; non-dev →
  `Platform_MessageBox`).
- Guard: `if( host.status == HOST_SHUTDOWN ) return;`
- `static qboolean recursive` — if already recursing → `Sys_Error` (fatal).
- Set `recursive = true`, copy to `hosterror2`, `host.errorframe = host.framecount`.
- Cleanup: `COM_InitHostState()`, `Cbuf_Clear()`, `SV_Shutdown(...)`,
  `CL_Drop()`, `CL_ClearEdicts()`, `Mod_FreeAll()`.
- `recursive = false`, then `Host_AbortCurrentFrame()` →
  `longjmp( g_abortframe, 1 )` (`host_state.c:189`) — unwinds to the
  `COM_Frame` `setjmp` and drops the rest of the tick.

Two `jmp_buf`s total: `g_abortframe` (per-frame abort, jumps to `COM_Frame`) and
`return_from_main_buf` (clean exit, jumps to `Host_Main`). The recursion +
`errorframe == framecount` double-error guard (rewrite Quirk Q-4) exists because
game DLLs sometimes throw two errors back-to-back during map-load failure and
the engine relies on the second being fatal.

**`xash3dpp` mapping (OQ-1, no `setjmp`/`longjmp`).** Stack-jumping over C++
destructors is UB, so the rewrite replaces both `jmp_buf`s:

- The **clean-exit** `longjmp` → normal loop exit via `HostStatus::Shutdown`.
- The **per-frame abort** `longjmp` → `Host::signal_frame_abort(ErrorCode,
  detail)` sets `frame_abort_pending` + records `frame_abort_code` and a bounded
  `frame_abort_detail` (no heap, `std::array`); the **next** `RunFrame` polls the
  flag at frame top and runs recovery, so every destructor from the aborted frame
  has already executed. Engine-internal callers instead propagate
  `std::expected<void, ErrorCode>` and `RunFrame` short-circuits (design; the
  per-frame entry points are Chunk 6/12).
- The **recursion guard** is preserved as-built: a second `signal_frame_abort`
  in the same pending window calls `platform::crash::print_trace()` then
  `XASH_FATAL(false, …)` (process abort) — the typed analog of the legacy
  `Sys_Error("…Recursive")` escalation.
- The **game-DLL C ABI** `Host_Error(const char*, ...)` survives verbatim as a
  `GAME_EXPORT` direct-symbol shim (rewrite Quirk Q-14 / OQ-10) in
  `xash3dpp_abi`, reaching the live host via
  `xash::abi::current_engine_context()`.

______________________________________________________________________

## 5. Feature flags — `-bugcomp` bitfield + the cvar roster

`host.bugcomp` (rewrite Quirk Q-11) is a `uint32_t` parsed **once** at boot from
`-bugcomp peoei,gsmrf,sp_attn_none,get_game_dir_full,…` and never mutated. Each
`BUGCOMP_*` bit gates a single behaviour divergence at its consumption site;
there is no central dispatcher. The host cvar roster (`host.c:66–82`) mixes
lifecycle/policy cvars (`host_developer`, `host_gameloaded`,
`host_clientloaded`, `host_limitlocal`, `host_allow_materials`, …) with the
seven **timing** cvars (`host_maxfps`/`fps_max`, `fps_override`,
`host_framerate`, `host_sleeptime`/`sleeptime`, `host_sleeptime_debug`,
`sys_timescale`, `sys_ticrate`).

Other frame-gate quirks worth preserving (all in `host.c`): the
singleplayer-and-not-demo `host_framerate` override gate (Quirk Q-7,
`host.c:645`), the hardcoded GoldSrc `31.0` connect-handshake FPS cap (Quirk
Q-8), and dedicated-server hibernation (`sv_hibernate_when_empty` → 500 ms sleep,
Quirk Q-9).

**`xash3dpp` mapping.** `bugcomp` is parsed by the launcher into
`HostArgs::bugcomp`, copied to `EngineContext::bugcomp` at init, and read
directly by each subsystem (OQ-7 "centralised parse, distributed consumption").
The **timing** cvars moved to `core::Clock::init` (see `deep-dive-core.md` §1.1,
including the `fps_max`/`sleeptime` public-name drift); the **lifecycle/policy**
cvars are host's to register but are **not yet built** (`TODO Chunk 3` in
`host.cpp`). The `host_framerate` singleplayer gate is preserved via
`Clock::set_frame_rate_gate` (OQ-11), inert until Chunk 6 wires the predicate.

______________________________________________________________________

## 6. As-built mapping (legacy → `xash3dpp/host`)

| Legacy construct | Where it lives now | Notes |
|------------------|--------------------|-------|
| `host_parm_t host` (god-struct) | split — see boundary spec *Owned state* | timing→`core::Clock`; FSM→`map_loader`; DLL paths→server/client; abort→`Host::Impl` |
| `Host_Main` | `Host::Main` | `while(status!=Shutdown) RunFrame()`; no `longjmp` trampoline |
| `Host_InitCommon` + phase ordering | `EngineContext::init` | declaration-order + explicit sequence; rollback-on-failure unwinds in reverse |
| `Host_Frame` fixed call order | `Host::RunFrame` | Q-1 order; only cbuf+map-step wired as-built |
| `host_status_t host.status` | `HostStatus` (`Init/Running/Sleep/Shutdown`) | fatal/crashed fold into abort + platform crash |
| `host_state_t` FSM (`host_state.c`) | `MapLoader` | `run_frame_step`, observers; host keeps only the driver call |
| `Host_Error` (engine-internal) | `Host::signal_frame_abort` + `std::expected` | OQ-1 hybrid; no `setjmp`/`longjmp` |
| `Host_Error` (game DLL `GAME_EXPORT`) | `xash3dpp_abi` C shim → `signal_frame_abort` | Q-14/OQ-10; frozen C signature |
| `recursive` + `errorframe==framecount` guard | `frame_abort_pending` recursive check → `XASH_FATAL` | Q-4 escalation preserved |
| `g_abortframe` / `return_from_main_buf` jmp_bufs | **removed** | flag-poll + normal loop exit |
| `Host_ExitInMain` / `Host_ShutdownWithReason` | `Host::RequestShutdown` | sets `Shutdown`; loop exits |
| `host.bugcomp` | `HostArgs::bugcomp` → `EngineContext::bugcomp` | OQ-7 parse-once/read-direct |
| timing cvars (`host.c:77–81`) | `core::Clock::init` | see `deep-dive-core.md` §1.1 |
| lifecycle/policy cvars + `quit`/`memlist`/`host_error` cmds | `TODO Chunk 3` (host) | **not yet built** as-built |
| `host` mempool | `Host::Impl::pool` | first created / last destroyed; standalone mode owns `own_fs` too |
| single global `host` | `abi::g_engine_ctx` (`std::atomic<EngineContext*>`) | the ONE sanctioned global (Q-2/OQ-10), hosted in `xash3dpp_host` after the D-1 dependency split |

______________________________________________________________________

## 7. New-in-rewrite (no legacy analog)

- **`EngineContext`** — the flat dependency-ordered owner; the legacy engine had
  only the `host` god-struct + scattered file-scope subsystem state.
- **`HostInitParams` / dependency injection** — legacy subsystems reached each
  other through globals; the rewrite injects non-owning nullable pointers
  (nullable = standalone/test mode).
- **`ThreadRole::Main` enforcement** — `assert_thread_role` at the lifecycle
  entry points; the legacy engine had no thread-role concept (see
  `deep-dive-core.md` §5 — the registry itself is core-owned, *established* by
  host).
- **The D-1 accessor split (2026-07-06)** — the `set/current_engine_context`
  *state* now lives in `xash3dpp_host` (`engine_context_accessor.cpp`) and the
  `xash3dpp_abi` shim only *consumes* it, breaking the historical host↔abi link
  cycle. The header keeps its `xash3dpp/abi/` path + `xash::abi` namespace.
