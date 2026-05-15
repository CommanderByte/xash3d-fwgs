# platform — Architecture Overview

> **Source**: `xash3dpp/src/platform/`\
> **Public API**: `xash3dpp/include/xash3dpp/platform/`\
> **Legacy reference**: `engine/platform/win32/`, `engine/platform/posix/`, `engine/common/system.h`

## Purpose

`xash3dpp_platform` provides thin, compile-time-selected wrappers over the small
set of OS capabilities that every other xash3dpp subsystem needs: monotonic time,
thread sleep, dynamic library loading and symbol resolution, system path queries,
raw file I/O, developer console I/O, crash handling, and structured logging.
It is the lowest non-trivial layer in the xash3dpp dependency graph — only
`xash3dpp_utilities` sits below it.

The module explicitly does **not** own: windowing, GPU context creation, audio,
networking, clipboard, or high-resolution sleep (those belong to future host or
renderer subsystems).

## Design goals

- **Zero init/shutdown for most features**: every function in `platform.hpp`,
  `os_io.hpp`, `log.hpp`, and `crash.hpp` is usable before any subsystem
  `init()` is called — even from global constructors.
- **Header isolation**: platform-specific system headers (`<windows.h>`,
  `<unistd.h>`, `<android/asset_manager.h>`) are confined to `.cpp` files.
  Public headers include only the C++ standard library.
- **No heap allocation in hot paths**: `logf`, `print_trace`, and all OS I/O
  operations work with fixed-size stack buffers or caller-supplied memory.
- **No exceptions, no RTTI**: consistent with the engine-wide `/EHs-c- /GR-`
  policy. All failure paths return `{}`, `false`, `nullopt`, or `-1`.
- **Porting is additive**: adding a new platform requires creating one
  subdirectory with four mandatory source files (`sys.cpp`, `os_io.cpp`,
  `console.cpp`, `crash.cpp`) and one CMake `elseif` branch — no existing code
  changes.
- **Thread-safe defaults**: the default log sink and all OS file I/O functions
  are callable from any thread; main-thread-only operations are enforced at
  debug time via `assert_main_thread`.

## Key invariants

- `get_time()` is monotonic within a process lifetime. Its epoch is set on the
  first call via a C++11 magic-static and never reset. As a side effect, the
  first call also records the calling thread as the main thread.
- `OsFd` takes exclusive ownership of a file descriptor. Copying is deleted;
  a moved-from `OsFd` is left invalid (`fd_ == -1`).
- `crash::install_handler()` must be called from the main thread before worker
  threads are created. Calling it afterwards is undefined behaviour on POSIX
  (signal disposition is process-wide; `sigaction` must precede thread spawning).
- `log_set_callback()` must be called from the main thread before worker threads
  are spawned. The callback is invoked **in addition to** — not instead of — the
  default stderr/console sink.
- `console::read_line()` returns a `string_view` into a static buffer. The view
  is invalidated by the next call; callers must copy if persistence is required.
- All path-returning functions use forward slashes and append a trailing `/`.

## Relationship to legacy code

The legacy engine scattered OS abstractions across `engine/platform/win32/sys_win.c`,
`engine/platform/posix/sys_posix.c`, `filesystem/filesystem.c`, and
`engine/common/system.h`. The rewrite consolidates everything under one target
(`xash3dpp_platform`) with a single public header per feature area. Key
differences:

- File I/O moved from `filesystem/` into `platform/` — the filesystem layer is
  now a consumer of OS I/O rather than an owner.
- Logging replaces `Con_Printf`/`Con_DPrintf`/`Host_Error` with a structured
  `LogLevel`-tagged API that supports an optional callback hook.
- The crash handler replaces ad-hoc signal registration scattered across
  per-platform source files.
- `open_library` no longer walks export tables — that responsibility belongs to
  the `xash3dpp_utilities` dynlib helpers.

## Architecture at a glance

```text
 ┌───────────────────────────────────────────────────────────────┐
 │  Callers (filesystem, memory, utilities, engine subsystems)   │
 │                                                               │
 │  #include <xash3dpp/platform/platform.hpp>  (time, dynlib,…) │
 │           <xash3dpp/platform/os_io.hpp>     (file I/O)       │
 │           <xash3dpp/core/log.hpp>           (logging — core)  │
 │           <xash3dpp/core/assert.hpp>        (assertions — core)│
 │           <xash3dpp/platform/console.hpp>   (dev console)    │
 │           <xash3dpp/platform/crash.hpp>     (crash handler)  │
 └─────────────────────────────┬─────────────────────────────────┘
                               │
 ┌─────────────────────────────▼─────────────────────────────────┐
 │  xash3dpp_platform (STATIC library)                           │
 │                                                               │
 │  log.cpp ─── shared across all platforms                      │
 │  ┌───────────────────────┬───────────────────────────────┐    │
 │  │  win32/               │  posix/ + android/            │    │
 │  │  sys.cpp              │  sys.cpp                      │    │
 │  │  os_io.cpp            │  os_io.cpp                    │    │
 │  │  console.cpp          │  + android/os_io.cpp          │    │
 │  │  crash.cpp            │  console.cpp / android/…      │    │
 │  │                       │  crash.cpp  / android/…       │    │
 │  └───────────────────────┴───────────────────────────────┘    │
 └─────────────────────────────┬─────────────────────────────────┘
                               │ PRIVATE dependency
 ┌─────────────────────────────▼─────────────────────────────────┐
 │  xash3dpp_utilities                                           │
 │  (path::extract_dir, path::fix_slashes)                       │
 └───────────────────────────────────────────────────────────────┘
```

Every allocation returned by this layer is either on the stack, in a static
buffer, or in a `std::string` / `std::vector` owned by the caller.
`xash3dpp_memory` is not used here.

## Index of concepts

- [index.md](./index.md) — full file/symbol index
- [system-utils.md](./system-utils.md) — time, sleep, dynlib, paths, message_box, shell_execute
- [os-io.md](./os-io.md) — `OsFd`, `OpenMode`, file/directory I/O, Android AAsset bridge
- [logging.md](./logging.md) — `LogLevel`, `log`/`logf`/`log_va`, callback, truncation *(owned by `xash3dpp_core`)*
- [console.md](./console.md) — `console::write`, `console::read_line`, per-platform notes
- [crash.md](./crash.md) — `crash::install_handler`, `crash::print_trace`, signal/SEH
- [assertions.md](./assertions.md) — `XASH_ASSERT`, `XASH_FATAL`, `XASH_DEBUG_BREAK`, `assert_main_thread` *(owned by `xash3dpp_core`)*
