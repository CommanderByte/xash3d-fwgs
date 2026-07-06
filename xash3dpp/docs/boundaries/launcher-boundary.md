# Launcher Boundary Spec

> **Scope note.** `launcher` is a single-file process entry point
> (`src/launcher/main.cpp`), not a stateful subsystem in the Q-1 sense. It has
> no header, no `EngineContext` member, no init params, and no runtime state.
> This spec exists so the finish-subsystem checklist reads as recorded judgment
> rather than unfinished work.

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

The launcher **establishes** `ThreadRole::Main`: the process's first thread *is*
the engine main thread, and `register_thread_role(ThreadRole::Main)` is called
before any engine call so that every downstream `assert_thread_role(Main)` (host,
engine_context, map_loader, server, …) is meaningful. It then stays on that
thread for the whole `Host::Main` lifetime — there is no threading of its own.

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
