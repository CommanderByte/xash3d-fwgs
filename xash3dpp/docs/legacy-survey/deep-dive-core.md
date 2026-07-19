# Deep Dive: Legacy Origins of `xash3dpp/core` — Frame Timing, Logging, Assertions, Errors, Thread Roles

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy behaviours the `xash3dpp`
**core** subsystem distills — the `host.c` frame-timing loop and its cvars, the
`Con_Printf` / `Host_Error` / `Sys_Error` diagnostic paths, and the legacy
`ASSERT` macros — plus an honest accounting of what in core is **new in the
rewrite** with no legacy analog (the `ErrorCode` vocabulary and the entire
`ThreadRole` registry). Line numbers are against the working tree on that date;
behaviour references, not design constraints. Everything about the legacy is
**reference-only**.*

Unlike the platform / filesystem deep-dives, core is **not** a port of a single
legacy directory: it is a *distillation* assembled from pieces of
`engine/common/host.c`, `engine/common/con_*.c`, and the SDK-wide assert
macros, joined by two constructs the legacy engine never had. The as-built
mapping (§6) is explicit about which is which.

Primary legacy sources:

- `engine/common/host.c` — the frame loop (`Host_FilterTime`, `Host_CalcFPS`,
  `Host_CalcSleep`, `Host_Frame`), `host.realtime` / `host.frametime` /
  `host.realframetime` / `host.framecount`, and the timing cvar roster
- `engine/common/con_utils.c` / the `Con_Printf` / `Con_DPrintf` / `Msg` family
  (console + log output)
- `engine/common/host.c` `Host_Error` and `engine/common/sys_con.c` `Sys_Error`
  (fatal paths)
- SDK `ASSERT` / `Assert` macros (`common.h` / `port.h`)

**Global assumptions (legacy):** all timing and console state is
**process-global** (`host_t host;` + file-scope cvars); there is no context
object and no thread-role concept — the engine assumes one main thread plus a
handful of ad-hoc helper threads (sound mixer, HTTP) that never call the
diagnostic or timing paths under a contract.

______________________________________________________________________

## 1. Frame timing — `host.c` (the `Clock` source material)

The legacy timing state lives on the global `host_t`:

| Legacy field | Meaning | `xash3dpp` as-built |
|--------------|---------|---------------------|
| `host.realtime` | accumulated scaled time since first frame | `Clock` `realtime_` (`atomic<double>`) |
| `host.frametime` | this frame's delta, possibly cvar-overridden | `Clock` `frametime_` |
| `host.realframetime` | raw clamped wall-clock frame delta | `Clock` `realframetime_` |
| `host.framecount` | accepted-frame counter | `Clock` `framecount_` (`atomic<uint64_t>`) |
| *(none — new)* | raw unscaled frame delta | `Clock` `pureframetime_` |
| *(none — new)* | platform time at init | `Clock` `starttime_` |

The gating logic the rewrite ports verbatim:

- **`Host_CalcFPS`** (`host.c:503`) — picks the target FPS: dedicated →
  `sys_ticrate.value`; client → `host_maxfps.value` with the
  `fps_override ? MAX_FPS_HARD : MAX_FPS_SOFT` ceiling, clamped
  `bound( MIN_FPS, fps, max_fps )`. Reproduced exactly in `clock.cpp`
  `calc_fps()` (with the `gl_vsync == 0` assumption pending Chunk 9).
- **`Host_FilterTime`** — accumulates `host.realtime += dt * timescale`,
  enforces the frame budget (`elapsed < 1/fps` → not ready), applies the
  `host_framerate` singleplayer-no-demo override, and clamps the delta.
  Reproduced in `Clock::tick()` (the OQ-11 gate replaces the inline
  singleplayer/demo test with an injected `gate_fn`).
- **`Host_CalcSleep`** (`host.c:~354`) — returns `host_sleeptime.value` ms to
  sleep when the budget is not yet reached. Reproduced as the `host_sleeptime`
  read + `platform::sleep` in `tick()`.
- **Dedicated +1 FPS** — legacy targets `1/(fps+1)` in dedicated mode to avoid
  being fractionally early; `tick()` carries the same `+1.0`.

### 1.1 Timing cvar roster (`host.c:67–81`)

The seven cvars `Clock::init()` registers, and their legacy definitions —
note two **public-name** differences the rewrite chose to drop:

| Rewrite cvar name | Legacy definition | Legacy public name | Flags |
|-------------------|-------------------|--------------------|-------|
| `host_maxfps` | `CVAR_DEFINE( host_maxfps, "fps_max", "72", … )` | **`fps_max`** | `FCVAR_ARCHIVE\|FCVAR_FILTERABLE` |
| `fps_override` | `CVAR_DEFINE_AUTO( fps_override, "0", … )` | `fps_override` | `FCVAR_FILTERABLE` |
| `host_framerate` | `CVAR_DEFINE_AUTO( host_framerate, "0", … )` | `host_framerate` | `FCVAR_FILTERABLE` |
| `host_sleeptime` | `CVAR_DEFINE( host_sleeptime, "sleeptime", "1", … )` | **`sleeptime`** | `FCVAR_ARCHIVE\|FCVAR_FILTERABLE` |
| `host_sleeptime_debug` | `CVAR_DEFINE_AUTO( host_sleeptime_debug, "0", … )` | `host_sleeptime_debug` | `0` |
| `sys_timescale` | `CVAR_DEFINE_AUTO( sys_timescale, "1.0", … )` | `sys_timescale` | `FCVAR_FILTERABLE` |
| `sys_ticrate` | `CVAR_DEFINE_AUTO( sys_ticrate, "100", … )` | `sys_ticrate` | `FCVAR_SERVER` |

**Parity note:** the rewrite registers cvar **names** `host_maxfps` and
`host_sleeptime` (the legacy C-symbol names), whereas the legacy engine exposes
them to users/configs as **`fps_max`** and **`sleeptime`**. If GoldSrc config
compatibility (`fps_max` in `config.cfg`) is required, this is a naming gap to
close at host bring-up. The rewrite also maps `FCVAR_FILTERABLE` onto plain
flags today (`sys_timescale` uses `FCVAR_CHEAT` in the rewrite vs
`FCVAR_FILTERABLE` in legacy) — a flag-semantics drift worth reconciling when
the filter/cheat cvar policy lands.

### 1.2 Constants (`MIN_FPS` / `MAX_FPS_SOFT` / `MAX_FPS_HARD`)

Legacy `host.c` uses `MIN_FPS`, `MAX_FPS_SOFT` (200), `MAX_FPS_HARD` (1000)
macros. The rewrite promotes these to `::xash::limits::{min_fps, max_fps_soft,
max_fps_hard}` and reads them as absolute references (constant classification
Q-O — no local literals).

______________________________________________________________________

## 2. Logging — `Con_Printf` / `Con_DPrintf` / `Msg` (the `log.cpp` source material)

Legacy console output is a family of variadic C functions scattered across the
engine (`Con_Printf`, `Con_DPrintf`, `Con_Reportf`, `Msg`, `MsgDev`), each
formatting into a stack buffer and routing to the platform console + the
in-game console buffer, with severity encoded by inline `S_ERROR` / `S_WARN` /
`S_USAGE` **string prefixes** (e.g. `Con_Printf( S_ERROR "%s", buf )`).

The rewrite collapses this to **one** severity-typed surface:

- `core::log(level, tag, text)` / `logf(level, tag, fmt, …)` / `log_va(…)`
  replace the whole `Con_*`/`Msg*` family.
- The `S_ERROR` / `S_WARN` string prefixes become the `LogLevel` enum
  (`Verbose` / `Info` / `Warning` / `Error` / `Fatal`), rendered as the
  `[tag][LEVEL]:` prefix — deliberately echoing the legacy `Con_Printf` prefix
  shape users recognise.
- `Con_DPrintf` (developer-only) maps to `LogLevel::Verbose`, compiled out
  unless `XASH_VERBOSE` is defined (legacy gated on `host_developer.value`; the
  rewrite gates at compile time to remove release I/O pressure).
- The legacy in-game console buffer + the platform console are unified behind
  **`platform::console::write`** (the default sink) plus an optional
  **`log_set_callback`** hook — the seam a future diagnostics / MCP / REPL
  frontend installs (this is the explicit **G-1** hook named in
  `extension-goals.md`). Legacy had no such single interception point.
- **Zero-heap / `noexcept`**: legacy `Con_Printf` could allocate for the
  history buffer; the rewrite sink uses a fixed `limits::platform_log_buffer_size`
  stack buffer with `vsnprintf` + truncation marker, callable from any thread
  and from a crash path.

______________________________________________________________________

## 3. Assertions — legacy `ASSERT` / `Assert` (the `assert.hpp` source material)

The legacy SDK exposes `ASSERT(x)` / `Assert(x)` macros that, in debug builds,
call into a break/abort path (and on some targets `Sys_Error`). There is no
severity split and no guaranteed diagnostic log.

The rewrite formalises this into a **two-tier** policy:

- `XASH_ASSERT(expr)` — debug-only (`NDEBUG` no-op), fires the platform
  debugger break directly, **no** log call (cheap by-construction invariants).
- `XASH_FATAL(expr, msg)` — always-on, logs via `core::logf(LogLevel::Fatal,…)`
  then aborts (data-corruption / porting-bug invariants).

Both are documented as distinct from Q-5 `ErrorCode` returns (invariants vs
recoverable failures) — a distinction the legacy `ASSERT` + `qboolean`-return
mix did not draw cleanly. `<cassert>` `assert()` is explicitly banned (MSVC
`/EHs-c-` `_invoke_watson` inconsistency; no diagnostic log).

______________________________________________________________________

## 4. Fatal paths — `Host_Error` / `Sys_Error` (the `ErrorCode` context)

Legacy fatal handling is `Host_Error( fmt, … )` (recoverable-to-menu via
`longjmp` back to the host frame) and `Sys_Error( fmt, … )` (unrecoverable,
message box + abort). Both are **string-formatted, control-flow** mechanisms —
there is **no** structured error-code vocabulary; functions return `qboolean`
or a raw pointer and signal failure out-of-band.

`core::ErrorCode` is therefore **new in the rewrite** (§6): a committed-integer
enum (`InvalidArgument`, `OutOfMemory`, `HostFatal`, `FrameAborted`,
`MapNotFound`, `Bsp*`, …) returned by value at subsystem boundaries, replacing
the legacy `qboolean` + `longjmp` idiom with typed returns. `HostFatal` /
`FrameAborted` are the codes that stand in for the `Host_Error` / frame-abort
`longjmp` targets. The legacy `abi/` shim keeps a `Host_Error` entry point for
the frozen game-DLL ABI, but internally maps onto the typed path.

______________________________________________________________________

## 5. Thread roles — **NEW in the rewrite (no legacy analog)**

The legacy engine has **no** `ThreadRole` concept. It assumes a single main
thread that runs the frame loop and calls into game DLLs, plus a small number
of ad-hoc helper threads (the SDL/OpenAL audio mixer callback, the HTTP
download worker) that are *not* governed by any declared contract and never
call the timing or (safely) the console paths.

`core::thread_role` is a **rewrite-native** primitive:

- a `thread_local ThreadRole tls_role` per thread (`thread_role.cpp`);
- `register_thread_role` (stamp the calling thread once) /
  `current_thread_role` / `assert_thread_role(expected)` (hard `XASH_FATAL` on
  mismatch, logging both role names);
- the `ThreadRole` enum (`Unknown`, `Main`, `AudioCallback`, `AudioDecoder`,
  `Worker`, `Render` [planned], `NetIO` [planned]) with **committed numeric
  values** (debug logs include the integer; new roles append only).

This is the **enforcement primitive** every subsystem's Main-only contract
asserts through, and the door behind every north-star off-main goal
(`extension-goals.md` G-1 / G-2 / G-3). The legacy `assert_main_thread()`
helper kept for platform code is a thin wrapper over
`assert_thread_role(ThreadRole::Main)`. See `docs/design/threading-model.md`
§5/§8 for the full role model. **Ownership split (carry-in):** core *defines
and enforces* roles; **platform** will own the OS **thread-spawn +
registration** wiring (its Q-21 headline door — currently a missing primitive).

______________________________________________________________________

## 6. As-built mapping (legacy origin → `xash3dpp/core`)

Honest NEW-vs-distilled accounting. "Distilled" = behaviour ported from a named
legacy construct; "**NEW**" = no legacy analog.

| Legacy construct | Site | `xash3dpp/core` as-built | New or distilled |
|------------------|------|--------------------------|------------------|
| `host.realtime`/`frametime`/`realframetime`/`framecount` on global `host_t` | `host.c` | `Clock::Impl` atomic fields (`realtime_`/`frametime_`/`realframetime_`/`framecount_`) | Distilled (+ new `pureframetime_`/`starttime_`) |
| `Host_CalcFPS` (dedicated `sys_ticrate` / client `host_maxfps`+`fps_override`) | `host.c:503` | `calc_fps()` (`clock.cpp`) — same clamps, `MAX_FPS_SOFT/HARD` | Distilled verbatim |
| `Host_FilterTime` (accumulate, budget, `host_framerate`, timescale, clamp) | `host.c` | `Clock::tick()` | Distilled; singleplayer/demo test → injected `gate_fn` (OQ-11) |
| `Host_CalcSleep` (`host_sleeptime` ms) | `host.c:~354` | `host_sleeptime` read + `platform::sleep` in `tick()` | Distilled |
| Timing cvar roster (7) | `host.c:67–81` | registered in `Clock::init()` via `CmdCvarContext` | Distilled; **names `host_maxfps`/`host_sleeptime` differ** from legacy public `fps_max`/`sleeptime` (§1.1) |
| `MIN_FPS`/`MAX_FPS_SOFT`/`MAX_FPS_HARD` macros | `host.c` | `::xash::limits::{min_fps,max_fps_soft,max_fps_hard}` | Distilled (promoted to limits) |
| `Con_Printf`/`Con_DPrintf`/`Msg` + `S_ERROR`/`S_WARN` prefixes | `con_utils.c` etc. | `core::log`/`logf`/`log_va` + `LogLevel` enum + `[tag][LEVEL]:` prefix | Distilled + unified (one surface, severity-typed) |
| in-game console buffer + platform console dual output | `con_*.c` | `platform::console::write` default sink + `log_set_callback` hook | Distilled; **callback hook is NEW** (G-1 seam) |
| `ASSERT`/`Assert` macros | `common.h`/`port.h` | `XASH_ASSERT` (debug break) / `XASH_FATAL` (always-on, logs) | Distilled + formalised into two tiers |
| `Host_Error` (`longjmp`) / `Sys_Error` (abort) | `host.c` / `sys_con.c` | `XASH_FATAL` + typed `ErrorCode::HostFatal`/`FrameAborted` returns | Distilled behaviour; **structured codes are NEW** |
| structured error vocabulary | *(none)* | `ErrorCode` enum + `error_code_name()` | **NEW** (replaces `qboolean` + out-of-band signalling) |
| thread-role registry + `assert_thread_role` | *(none)* | `thread_role.hpp` / `thread_role.cpp` (`ThreadRole`, `tls_role`) | **NEW** — the enforcement primitive |
| `assert_main_thread()` legacy helper | platform code | thin wrapper over `assert_thread_role(ThreadRole::Main)` | Distilled shim |

**Build-graph note (D-1):** although these all carry the `xash::core`
namespace and `xash3dpp/core/` header paths, only `clock.cpp` + `error.cpp`
compile into `xash3dpp_core`; `log.cpp` + `thread_role.cpp` (the diagnostics
tier) compile into `xash3dpp_platform` to break the historic core ⇄ platform
link cycle. Layering stays one-way (core → platform). See
`docs/boundaries/core-boundary.md` *As-built reconciliation*.
