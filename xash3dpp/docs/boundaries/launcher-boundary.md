# Launcher Boundary Spec

> Refreshed 2026-07-06 (as-built pass). Re-read `src/launcher/main.cpp` +
> `src/launcher/CMakeLists.txt` against this spec. The file matches the
> contract below; the one drift is that the NVIDIA Optimus / AMD PowerXpress
> export symbols documented as the launcher's home are **not yet present** in
> `main.cpp` (aspirational — see As-built reconciliation). Threading and a new
> `## Extension axes (Q-21)` section are added/refreshed this pass.

> **Scope note.** `launcher` is a single-file process entry point
> (`src/launcher/main.cpp`), not a stateful subsystem in the Q-1 sense. It has
> no header, no `EngineContext` member, no init params, and no runtime state.
> This spec exists so the finish-subsystem checklist reads as recorded judgment
> rather than unfinished work.

______________________________________________________________________

## As-built reconciliation (2026-07-06)

Re-scanned the one shipped TU (`src/launcher/main.cpp`, ~95 lines) and its
`CMakeLists.txt`. The spec and the code agree on every substantive point; the
notes below pin the exact as-built shape and the two aspirational gaps.

- **Argv contract matches.** `main.cpp` scans exactly the five documented
  options — `-game` (default `"valve"`), `-basedir` (defaults to `-game`),
  `-rodir` (default `""`), `-dedicated` (flag), `-dev [level]` (`std::atoi`,
  default unset) — into a stack `HostArgs`, then constructs `xash::Host` and
  returns `host.Main(args)`. No heap, no getopt. The two argv helpers
  (`get_arg`, `has_flag`) are file-`static`, pure, `noexcept`, and compare with
  `std::strcmp` over the raw `char** argv` (C-string in, C-string out).
- **`rootdir` from platform.** `args.rootdir = platform::get_executable_dir()`
  — the one platform call, as specified.
- **Role registration is first.** `core::register_thread_role(ThreadRole::Main)`
  is the first statement in the entry body, before any `HostArgs` fill or `Host`
  construction (the file's single hard invariant — see Threading / Quirks).
- **Target as specified.** `CMakeLists.txt` builds the `xash3dpp` executable,
  `cxx_std_23`, links `xash3dpp_host` **statically** (no launcher⇄engine DLL
  ABI), and sets `WIN32_EXECUTABLE TRUE` (so `WinMain` is the Win32 entry).
- **Drift — Optimus/PowerXpress exports not yet present.** The spec's ABI and
  Quirks sections name the launcher `.exe` as the required home for the
  `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` `dllexport`
  symbols (legacy `game_launch/game.cpp:35`). `main.cpp` does **not** define
  them yet — an aspirational completeness gap, not a contract change (recorded
  so the port is not assumed done).
- **Drift — SDL2 preload / Sailfish env not here (by design).** The legacy
  launcher's `LoadLibraryEx("SDL2.dll")` availability check and Sailfish
  `XASH3D_BASEDIR`/`RODIR` `setenv` are **not** in `main.cpp`; per
  `host-boundary.md` § "What the launcher does today" they move to
  `platform` / build-time search path. As-built the launcher stays minimal.
- **Still Partial — no unit suite.** `tests/launcher/` still does not exist; the
  argv→`HostArgs` behaviour is covered end-to-end (host + hl-smoke). This is a
  recorded judgment (see Tests), the subsystem's only open "Partial" item.

## Responsibility

The launcher is the **process entry point**. It does three things and nothing
else:

1. **Register the engine main thread** — `register_thread_role(ThreadRole::Main)`
   is the very first statement, before any engine call (QN prerequisite:
   `assert_thread_role(Main)` is fatal on an unregistered thread).
2. **Parse argv → `HostArgs`** — resolve `rootdir` from the executable path and
   scan the standard engine options (`-game`, `-basedir`, `-rodir`,
   `-dedicated`, `-dev`) into a `HostArgs` with no heap allocation.
3. **Run the engine** — construct a `Host` and call `Host::Main(args)`, which
   blocks until shutdown, and return its exit code.

It does **not** contain engine logic, own any subsystem, load DLLs, or manage
windows. Per the host boundary's launcher split, the contract is:
**launcher = arg parsing + envvar/rootdir resolution + (platform) exports;
host = everything else.**

## ABI

None of its own. On Windows it provides the `WinMain` entry (and is the natural
home for NVIDIA Optimus / AMD PowerXpress export symbols); on POSIX the standard
`main`. It links `xash3dpp_host` statically — there is no launcher⇄engine DLL
boundary in the rewrite (the legacy `Host_Main`/`Host_Shutdown`/`pfnChangeGame`
loader ABI dissolves; see host-boundary.md § External ABI contracts).

## Interface

- **Input:** `argc` / `argv` (Win32: `__argc` / `__argv`).
- **Output:** process exit code from `Host::Main`.
- **Consumes:** `xash::Host` / `xash::HostArgs` (`host`),
  `xash::platform::get_executable_dir()` (`platform`),
  `xash::core::register_thread_role` (`core`).

## Dependencies

| Dependency | Why |
|---|---|
| `host` | constructs `Host`, fills `HostArgs`, calls `Host::Main` |
| `platform` | `get_executable_dir()` for `rootdir` resolution |
| `core` | `register_thread_role(ThreadRole::Main)` at entry |

## Owned state

None. Consequently:

- **Stats (reviewer §5):** `stats exempt` — no mutable state, no per-frame path
  (recorded in `main.cpp`).
- **Q-11 satellite (reviewer §10 / finish §9):** **not a satellite.** The
  launcher is the top-level process entry, not a driver/satellite of another
  subsystem; the compat/socket scanners report zero findings, so no satellite
  scoring applies.
- **Limits / cvars (QO):** the launcher registers no cvars and defines no
  capacity constants; the option strings it scans are legacy-frozen flag names
  owned by the parsing contract, not `limits.hpp` entries.

## Threading

> Refreshed 2026-07-06 (as-built pass).

The launcher **establishes** `ThreadRole::Main`: the process's first thread *is*
the engine main thread, and `register_thread_role(ThreadRole::Main)` is called
before any engine call so that every downstream `assert_thread_role(Main)` (host,
engine_context, map_loader, server, …) is meaningful. It then stays on that
thread for the whole `Host::Main` lifetime — there is no threading of its own.

**Classification (analyse-threading).** The launcher is a **pre-thread,
single-threaded bootstrap** — it runs before any worker/OS thread exists and
spawns none. As-built it has:

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| entry (`WinMain` / `main`) body | `src/launcher/main.cpp` | **Establishes-Main / single-threaded bootstrap** | Runs on the process's first (and only) thread; calls `register_thread_role(Main)` once, then `Host::Main`. No concurrency of its own |
| `register_thread_role(Main)` | `main.cpp` (first stmt) | **Role provider (write-once)** | The 1 threading call in the subsystem. Must precede any engine call so downstream `assert_thread_role(Main)` is valid |
| `get_arg` / `has_flag` | `main.cpp` (file-`static`) | **Pure / role-agnostic** | `noexcept`, no state, `std::strcmp` over `argv`; race-free (no shared mutable state) |

There are **zero** `assert_thread_role` sites (nothing to enforce — the launcher
is the *provider* of Main, not a consumer) and **zero** file-scope mutable
statics. No mutex, no atomic, no `setjmp`/`longjmp`. Step-5 signal reachability
is N/A (the launcher installs no handlers; crash/signal wiring is `platform` +
`host`).

______________________________________________________________________

## Extension axes (Q-21)

> Added 2026-07-06 (as-built pass). Evaluated against
> `docs/design/extension-goals.md`. The launcher is a **door-neutral** bootstrap
> — it owns no state, no seam, and no ABI, so almost every axis is **N/A**. The
> one live door is the **load-time flavor / mode-select** switch: the argv it
> parses is the natural place a future engine flavor (G-2) or service mode
> (G-1) is selected before the engine exists.

| Goal / primitive | Applies? | Verdict / door |
|------------------|----------|----------------|
| **G-1** in-engine MCP service | **Door (thin)** | Dedicated-server-first is the MCP v0 posture (extension-goals §G-1). The launcher already parses `-dedicated`; a future `-mcp` (or equivalent) service-mode switch would be parsed here and handed to `Host` via a `HostArgs` field. **Door-keep rule:** add such switches as typed `HostArgs` fields, never as launcher-side global state or engine reach-around |
| **G-2** game ABI v2 (load-time flavor) | **Door (thin)** | Q-20 frames a v2 ABI as a **load-time flavor** chosen once at process start. If flavor selection ever becomes a command-line concern (`-flavor`/`-sdk2`), argv→`HostArgs` is where it enters. Today: no such switch; the launcher only records the door so a flavor flag lands as a `HostArgs` field, not a launcher branch |
| **P-3** context-first, no new file-scope state | **Yes (trivially held)** | The launcher owns **zero** file-scope mutable state; `HostArgs` is a stack value handed by value to `Host`. The door-keep rule is simply *keep it that way* — any new option becomes a `HostArgs` field, never a launcher global |
| **P-1** main-thread inbox / worker pool | N/A | Pre-thread bootstrap; no off-main surface. It *provides* `ThreadRole::Main` that P-1's Main-drain later relies on, but owns no queue |
| **P-2** published-snapshot reads | N/A | No state to publish; nothing reads the launcher |
| **P-4** typed introspection | N/A | No state to introspect. (The flavor/mode switch above surfaces *into* `HostArgs`, which host owns — not a launcher query surface) |
| **P-5** narrowest-state signatures | **Yes (already)** | `get_arg`/`has_flag` take the narrowest inputs (`argc`, `argv`, key); the entry hands `Host` exactly one `HostArgs` |
| **P-6** services are satellites | N/A | The launcher is the top-level executable, not a satellite |
| **P-7** over-aligned / allocator hooks | N/A | No allocation |
| **G-3 / G-4 / G-5** | N/A | No debug-thread, overlay, or scripting surface; the launcher hands off before any such consumer exists |

**Net verdict:** the launcher owes **no new seam**. Its only extension relevance
is that it is the single point where a *load-time* flavor (G-2) or service-mode
(G-1) selection would enter — and the binding door-keep rule is that any such
selection is expressed as a typed `HostArgs` field (host-owned), preserving the
"launcher owns no state" invariant. Preservation, not new work.

## Quirks and invariants

- **Role registration is first, unconditionally.** `register_thread_role(
  ThreadRole::Main)` must run before *any* engine call; reordering it after
  `Host` construction or `HostArgs` fill would trip the first
  `assert_thread_role(Main)` fatally. This is the single hard invariant of the
  file.
- **`-basedir` defaults to `-game`.** When `-basedir` is absent, `basedir` is
  set equal to `gamedir` (legacy launcher default; `XASH_GAMEDIR` = `"valve"`).
- **Win32 entry is `WinMain`.** On Windows the entry is `WinMain` reading
  `__argc`/`__argv` and the target is built `WIN32_EXECUTABLE`; the `.exe` is
  also the required home for NVIDIA Optimus / AMD PowerXpress export symbols.
  POSIX uses the standard `main`.
- **No DLL boundary.** Unlike the legacy `game_launch` shim, the rewrite links
  `host` statically — there is no `LoadLibrary("xash.dll")` /
  `GetProcAddress("Host_Main")` step (host-boundary.md § External ABI contracts).

## Annotation discipline (Q-22 / QN)

Nothing to annotate: no headers, no raw-pointer members, no off-main surface.
`compliance_scan launcher --checks all/detail` = 0 violations and
`annotation-coverage` = 100 % on every axis (all denominators are 0). The
argv-helper free functions (`get_arg`, `has_flag`) are pure and `noexcept`.

## Tests (recorded judgment)

There is deliberately **no** `tests/launcher/` unit suite — the launcher is a
trivial argv→`HostArgs` shim whose only substantive behaviour (bring-up + frame
loop) is covered end-to-end by the server-lifecycle **hl smoke** integration
test (`tests/server/lifecycle/test_hl_smoke.cpp`) and by
`tests/host/test_host.cpp` exercising `HostArgs`/`Host::Main` internals.
`finish_check` item 6 therefore reports "0 test files" for the `launcher`
scope — a **recorded judgment**, not missing coverage.
