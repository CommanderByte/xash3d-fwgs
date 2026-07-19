# Platform Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> C++ standard in use: C++**23** (from `xash3dpp/src/platform/CMakeLists.txt`,
> `target_compile_features(xash3dpp_platform PUBLIC cxx_std_23)` — required for
> `std::expected<T, NetError>` in the sockets layer; the tree-wide
> `CMAKE_CXX_STANDARD 23`).
> Boundary spec: `docs/boundaries/platform-boundary.md`
> Threading: inline in the boundary spec (`## Threading`) — platform keeps its
> thread-role analysis in the boundary doc, not a separate file.
> ABI-frozen symbols in this subsystem: **None** — platform is fully internal
> (boundary spec §External ABI). No Game/Client DLL header exposes
> `xash::platform` symbols; the engine host will eventually fill legacy
> `enginefuncs_t` slots (e.g. `pfnLoadLibrary`) from here via a thin shim.

## Summary

The platform subsystem is small-per-OS but wide (14 TUs across
`win32` / `posix` / `android`) and already modern: `std::string_view` inputs,
`std::optional` returns, `enum class OpenMode` bit-flags, RAII handle types
(`OsFd`, `OsSocket`, `LibHandle`), `std::expected<T, NetError>` on the socket
surface, magic-static clock init, `std::atomic` for the WSA refcount and
crash-installed flag, and `call_once`-guarded Android JNI glue.
`compliance_scan.py platform` is **clean** and `status_table.py` reports it
**Complete**.

Two facts drive this report:

1. **The OS file I/O backend now lives here.** During the filesystem refresh,
   `src/filesystem/platform/{win32,posix}.cpp` were extracted into
   `xash3dpp_platform` (`os_io.hpp` + per-OS `os_io.cpp`). The filesystem
   **H-2** (fixed-size `wchar_t`/`char` path buffers) and **L-4**
   (`SEEK_SET`/`SEEK_CUR`/`SEEK_END` at the OS-call boundary) findings therefore
   **relocated** to this backlog. On re-scan, **H-2 is largely already done in
   the extracted code** — `os_io.cpp`'s `to_wide` uses the robust two-call
   `MultiByteToWideChar` pattern returning `std::optional<std::wstring>`, and
   `list_directory` sizes a `std::string` dynamically — so the relocated
   finding survives only as an **inconsistency** (below, **H-1**) and a typed-
   origin question (**M-1**).

2. **The `strnicmp`/`strncmp` over-read pattern is absent.** The utilities
   **M-4** / filesystem **M-7** latent `string_view` → C-string over-read does
   **not** occur in platform: a scan of all 14 TUs finds **zero** `strnicmp` /
   `strncmp` call sites. Platform's `string_view` inputs are either copied into
   a bounded buffer with an explicit terminator (`open_library`) or handed to a
   length-taking OS API (`MultiByteToWideChar(..., s.size(), ...)`). Reported
   clean.

What remains is a short tail: one latent path-truncation inconsistency in the
dynamic-library loader (High, correctness), a typed-origin promotion at the
`platform::seek` OS boundary (Medium, the relocated L-4), a couple of enum /
naming tidy-ups, and three feature-completeness gaps that are already tracked as
boundary-spec open questions or `stub_scan` TODOs.

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| OS file I/O backend (`os_io.hpp` + per-OS `os_io.cpp`) | **Implemented** | Absorbed from filesystem; carries the relocated H-2 / L-4 |
| `to_wide` two-call `MultiByteToWideChar` (`os_io.cpp`) | **Implemented** | The filesystem-H-2 fix **landed here** — returns `std::optional<std::wstring>`, no truncation |
| Socket layer + `IPlatformSockets` seam | **Implemented** | Q-7 test seam; `std::expected` result type |
| Diagnostics tier hosted in target (`core/log.cpp`, `core/thread_role.cpp`) | **Implemented** | D-1 dependency hardening; breaks the core ⇄ platform cycle |
| Consolidated UTF-8 → UTF-16 conversion (win32) | **Not implemented** | Three separate converters; `open_library`'s is truncating — **H-1** |
| Typed origin at `platform::seek` (vs raw `int whence`) | **Not implemented** | Relocated filesystem **L-4** — **M-1** |
| `OpenMode::create` casing / boundary-spec `Create` mismatch | **Not implemented** | Naming drift — **M-2** |
| `is_debugger_present` on macOS / BSD | **Not implemented** | `stub_scan` TODO (`posix/sys.cpp`) — **L-1** |
| `shell_execute` POSIX double-fork (zombie reaping) | **Not implemented** | `stub_scan` TODO (`posix/sys.cpp`) — **L-2** |
| High-resolution sleep (`Win32_NanoSleep` / `SDLash_NanoSleep`) | **Not implemented** | Boundary-spec open question — **L-3** |
| OS thread-spawn + `ThreadRole` registration primitive | **Not implemented** | Q-21 headline **door** (not a modernization item — see cross-cutting flags) |
| Clipboard / SIGTERM / Android extras | **Not implemented** | Boundary-spec open questions; deferred to host / Android target |
| `strnicmp` / `strncmp` `string_view` over-read | **N/A — absent** | 0 sites; utilities M-4 / filesystem M-7 pattern does not occur here |

______________________________________________________________________

## High-priority opportunities

### H-1: Consolidate the win32 UTF-8 → UTF-16 converters and eliminate the truncating `open_library` path (relocated filesystem H-2)

- **File(s)**: `xash3dpp/src/platform/win32/sys.cpp` (`open_library`,
  `get_executable_dir`, `get_working_directory`);
  `xash3dpp/src/platform/win32/os_io.cpp` (`to_wide`);
  `xash3dpp/src/platform/win32/os_socket.cpp` (`to_wide`).

- **Current pattern**: three independent UTF-8 → UTF-16 conversions coexist in
  the win32 platform TUs, and they do **not** agree on safety.
  `os_io.cpp` / `os_socket.cpp` use the robust two-call pattern (query length,
  then fill a right-sized `std::wstring`):

  ```cpp
  static std::optional<std::wstring> to_wide( std::string_view s ) noexcept; // os_io.cpp, os_socket.cpp
  ```

  but `open_library` still converts into a **fixed** stack buffer that silently
  truncates a long DLL path:

  ```cpp
  wchar_t wbuf[::xash::limits::platform_path_buf_wchars];   // 1024 wchars
  int len = MultiByteToWideChar( CP_UTF8, 0, path.data(),
      static_cast<int>( path.size() ), wbuf,
      static_cast<int>( std::size( wbuf ) ) - 1 );
  if( len <= 0 ) return {};                                 // long path → silent {}
  ```

  A UTF-8 DLL path whose UTF-16 form exceeds ~1023 wchars makes
  `MultiByteToWideChar` return 0 (insufficient buffer), so `open_library`
  returns a null handle with no diagnostic — the exact latent truncation the
  filesystem-H-2 fix eliminated everywhere else.

- **Suggested replacement**: promote the two-call `to_wide` to a single shared
  `platform` internal helper (e.g. an anonymous-namespace header in
  `src/platform/win32/`) and route `open_library` (and, for symmetry, the two
  `get_*_directory` paths, which already size their output dynamically) through
  it. Deletes two duplicate `to_wide` definitions and the last truncating path
  buffer.

- **Boundary-safe**: Yes — engine-internal; the observable contract
  (`{}` on failure) is unchanged, only the failure cause "path too long"
  disappears.

- **Rationale**: correctness (removes a silent-truncation footgun on the DLL
  loader — the one path that must never quietly fail for a valid mod) plus
  de-duplication of three copies of the same conversion. This is the surviving
  substance of the relocated filesystem H-2.

______________________________________________________________________

## Medium-priority opportunities

### M-1: Typed seek origin at the `platform::seek` OS boundary (relocated filesystem L-4)

- **File(s)**: `xash3dpp/include/xash3dpp/platform/os_io.hpp`
  (`seek( OsFd&, std::int64_t, int whence )`); per-OS `os_io.cpp` (forwards to
  `_lseeki64` / `lseek`); filesystem callers in `src/filesystem/file.cpp` /
  backends.

- **Current pattern**: the platform seek takes a raw POSIX `int whence`, so
  every caller still writes `platform::seek( fd, off, SEEK_SET )` and drags in
  `#include <cstdio>` purely for the macros:

  ```cpp
  [[nodiscard]] std::int64_t seek( OsFd &fd, std::int64_t offset, int whence ) noexcept;
  ```

  The filesystem layer already has a typed `enum class SeekOrigin { Begin,
  Current, End }` (its M-2, done) at the `File` API, but it degrades back to the
  raw `int` at this OS-call boundary.

- **Suggested replacement**: give the platform seek its own scoped origin enum
  (e.g. `enum class SeekWhence : int { Begin = SEEK_SET, Current = SEEK_CUR,
  End = SEEK_END };` in `os_io.hpp`) and take it by value. Callers pass
  `SeekWhence::Begin`; the `<cstdio>` include at the call sites disappears.
  Ownership of this decision moved to **platform** when the OS I/O extracted
  out of filesystem (the filesystem L-4 note explicitly hands it over).

- **Boundary-safe**: Yes — additive within `xash3dpp`; the enum values pin to
  the same OS constants, so no behavioural change.

- **Rationale**: closes the relocated L-4; makes the OS boundary self-describing
  and stops leaking libc macros into higher layers.

### M-2: `OpenMode::create` casing — align with the enum and the boundary spec

- **File(s)**: `xash3dpp/include/xash3dpp/platform/os_io.hpp` (`enum class
  OpenMode`); every `any( mode & M::create )` use in the per-OS `os_io.cpp`.

- **Current pattern**: the flag is spelled lowercase `create` while its siblings
  are PascalCase (`ReadOnly`, `WriteOnly`, `ReadWrite`, `Append`, `Truncate`,
  `Memory`), and the boundary-spec Interface table documents it as `Create`:

  ```cpp
  enum class OpenMode : std::uint32_t {
      ReadOnly = 0, WriteOnly = 1, ReadWrite = 2, Append = 4,
      create = 8,           // <-- lowercase outlier
      Truncate = 16, Memory = 32,
  };
  ```

- **Suggested replacement**: rename `create` → `Create` (and the paired
  `make_directory` comment "create the directory" is unrelated prose — leave).
  Pure mechanical rename across the three `os_io.cpp` files.

- **Boundary-safe**: Yes — enumerator is `xash3dpp`-internal; no ABI, no wire
  value change (the numeric value 8 stays).

- **Rationale**: removes the one casing outlier so the enum reads consistently
  and matches its own boundary-spec documentation. Cosmetic but cheap.

### M-3: POSIX `open_library` path copy → bounded helper (parallel to H-1)

- **File(s)**: `xash3dpp/src/platform/posix/sys.cpp` (`open_library`).

- **Current pattern**: the POSIX loader copies the `string_view` into a
  fixed `char buf[PATH_MAX]` with a manual clamp + terminator before `dlopen`:

  ```cpp
  char buf[PATH_MAX];
  std::size_t n = path.size() < sizeof( buf ) - 1 ? path.size() : sizeof( buf ) - 1;
  std::memcpy( buf, path.data(), n );
  buf[n] = '\0';
  void *h = dlopen( buf, RTLD_NOW | RTLD_LOCAL );
  ```

  Same silent-truncation class as H-1 (a path > `PATH_MAX-1` loads the wrong
  file or fails quietly), though `PATH_MAX` makes it far less likely than the
  win32 case.

- **Suggested replacement**: since `dlopen` needs a C string, the cleanest fix
  is a tiny `std::string tmp{ path }` (heap, cold path — dynlib load is not hot)
  passed as `tmp.c_str()`, which is always null-terminated and never truncates.
  Alternatively a shared `copy_bounded(std::span<char>, std::string_view)`
  helper if the stack copy is preferred.

- **Boundary-safe**: Yes.

- **Rationale**: same correctness argument as H-1 at lower urgency (`PATH_MAX`
  headroom); worth pairing so both loaders lose the truncation footgun together.

______________________________________________________________________

## Low-priority opportunities

### L-1: Implement `is_debugger_present` on macOS / BSD (stub TODO)

- **File(s)**: `xash3dpp/src/platform/posix/sys.cpp` (`is_debugger_present`).

- **Current pattern**: Linux reads `TracerPid` from `/proc/self/status`; every
  other POSIX target returns `false` with a `// TODO: implement for macOS
  (PT_ATTACHEXC) and BSD (ptrace).` marker (surfaced by `stub_scan.py platform`).

- **Suggested replacement**: macOS `sysctl(KERN_PROC, KERN_PROC_PID)` +
  `P_TRACED`; BSD `ptrace`/`kinfo_proc`. Boundary-spec quirk already documents
  the `false` fallback.

- **Boundary-safe**: Yes — behaviour-additive on non-Linux only.

- **Rationale**: developer-experience only (auto-break on crash under a
  debugger); genuinely low priority.

### L-2: POSIX `shell_execute` double-fork to reap zombies (stub TODO)

- **File(s)**: `xash3dpp/src/platform/posix/sys.cpp` (`shell_execute`).

- **Current pattern**: `fork` + `execvp` fire-and-forget with a `// TODO:
  replace with double-fork to avoid zombie accumulation on long runs.` marker.
  A never-`wait`ed child becomes a zombie until the engine exits.

- **Suggested replacement**: double-fork (fork → child forks the exec target and
  `_exit`s, parent `waitpid`s the middle child) so the grandchild is reparented
  to init and reaped by the OS.

- **Boundary-safe**: Yes.

- **Rationale**: matters only for very long dedicated-server uptimes that open
  many external URLs; rare in practice.

### L-3: High-resolution sleep primitive

- **File(s)**: `os_io.hpp` neighbours in `platform.hpp` (`sleep(ms)`); per-OS
  `sys.cpp`.

- **Current pattern**: `platform::sleep` is millisecond-granular (`Sleep(ms)` /
  `nanosleep` on a ms value). The legacy engine exposes sub-millisecond timing
  via `Win32_NanoSleep` (a `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` waitable
  timer) and `SDLash_NanoSleep` for the frame-timing loop.

- **Suggested replacement**: add a `sleep_precise(nanoseconds)` when the host
  frame-timing loop is built (boundary-spec open question). Defer until the host
  subsystem schedules it — no consumer today.

- **Boundary-safe**: Yes — additive.

- **Rationale**: frame-pacing accuracy; explicitly deferred to the host bring-up.

______________________________________________________________________

## Open questions (carried from the boundary spec)

- **Clipboard** (`Sys_GetClipboardData`) — in-game console paste. Own here or in
  a future `xash3dpp_window` subsystem?
- **SIGTERM handling** (`Posix_SetupSigtermHandling`) — catch here → a
  `host::request_quit()`, or own it in the host?
- **Android extras** (`Android_GetKeyboardHeight`, `Android_GetAndroidID`) —
  deferred until the Android build target is added.
- **High-resolution sleep** — see L-3; add with the frame-timing loop.

______________________________________________________________________

## Cross-cutting flags (for the Phase 14 synthesis)

- **Relocated filesystem H-2 / L-4 land here.** H-2 is *substantially already
  implemented* in the extracted `os_io.cpp` (two-call `to_wide`, dynamic
  `list_directory`); its residue is the win32 `open_library` truncation
  inconsistency (**H-1**) — flag that the "fix" existed in one TU but not the
  sibling dynlib loader, a classic extract-and-diverge. L-4 becomes platform's
  **M-1** (typed seek origin at the OS boundary).
- **`strnicmp` / `strncmp` over-read is ABSENT in platform** (0 sites). The
  utilities-M-4 / filesystem-M-7 `string_view` → C-string over-read does not
  recur here — platform either copies into a bounded terminated buffer or uses
  length-taking OS APIs. This is a *negative* data point for the sweep: the
  pattern is real but not universal.
- **The Q-21 headline is a missing primitive, not legacy debt.** Platform needs
  an OS **thread-spawn + `ThreadRole`-registration** primitive for the north
  star's off-main threads (G-1/G-3/NetIO/Worker). It already hosts
  `thread_role.cpp`, so it is the natural owner. This is a *door* (build it when
  Chunk 7's worker pool schedules the first consumer), not a modernization of
  existing code — recorded here so the synthesis pass can pair it with the P-1
  inbox design across subsystems.
- **Three UTF-8 → UTF-16 converters** in the win32 TUs is a small instance of a
  likely tree-wide theme (per-TU re-implementation of the same OS glue). Worth a
  one-line note in the synthesis if other subsystems show the same duplication.
