# Platform Boundary Spec

> Refreshed 2026-07-06 (as-built pass). Re-scanned all 14 TUs under
> `src/platform/{win32,posix,android}/**` and the public headers under
> `include/xash3dpp/platform/**`. `status_table.py` reports platform
> **Complete** (14 TUs, tests ✔); `compliance_scan.py platform` is **clean**
> (0 blocker/warning/note; 9 reviewer-judgment areas = the documented
> `compliance-allow` adjudications); `stub_scan.py platform` shows **2** TODO
> markers (both in `posix/sys.cpp`: `is_debugger_present` macOS/BSD, and a
> `shell_execute` double-fork note). Drift found this pass: the OS **file I/O**
> backend (`os_io.hpp` + per-OS `os_io.cpp`) is now owned here — it absorbed the
> filesystem **H-2 / L-4** items during the `src/filesystem/platform/{win32,
> posix}.cpp` extraction (see `modernization-opportunities/platform-modernization.md`).
> The socket surface (`os_socket` + `IPlatformSockets`) and the hosted
> diagnostics tier (`core/log.cpp`, `core/thread_role.cpp` built into this
> target) are reflected below. New sections added this pass:
> **Extension axes (Q-21)**; the **Threading** section gained a class table.

## Responsibility

Provides thin OS-abstraction wrappers for the handful of system capabilities
that every other xash3dpp subsystem needs: monotonic time, thread sleep,
dynamic library loading and symbol resolution, system path queries, error
dialogs, shell execution, raw file I/O, developer-console I/O, and crash
handling. Platform is the lowest non-trivial subsystem — only
`xash3dpp_utilities` sits below it in the dependency graph.

## External ABI contracts

None — fully internal. No Game DLL or client DLL header exposes
`xash::platform` symbols directly. The engine host will eventually fill legacy
`enginefuncs_t` function pointers (e.g., `pfnLoadLibrary`) from this module via
a thin shim.

## Interface (what the rest of the engine calls)

### System utilities (`xash::platform`)

| Symbol | Kind | Description |
|--------|------|-------------|
| `get_time()` | function | Monotonic elapsed seconds (double); epoch on first call |
| `sleep(ms)` | function | Yield current thread for ≥ ms milliseconds |
| `LibHandle` | struct | Opaque shared-library handle; default = null |
| `open_library(path)` | function | Load a shared library (UTF-8 path); returns `{}` on failure |
| `get_symbol(lib, name)` | function | Resolve exported symbol by name; returns nullptr on failure |
| `close_library(lib)` | function | Unload library and zero the handle; no-op if null |
| `get_executable_dir()` | function | Directory of the engine executable (forward slashes, trailing `/`) |
| `get_working_directory()` | function | Current working directory (forward slashes, trailing `/`) |
| `is_debugger_present()` | function | True if a debugger is attached; false on unsupported platforms |
| `message_box(title, msg)` | function | Modal error dialog; degrades to stderr on headless builds |
| `shell_execute(path, params)` | function | Open path with OS default handler; fire-and-forget |

### OS file I/O (`xash::platform`, `include/xash3dpp/platform/os_io.hpp`)

| Symbol | Kind | Description |
|--------|------|-------------|
| `OsFd` | class | RAII wrapper around a native fd (POSIX `int` or Win32 CRT fd). Non-copyable; movable. Calls `close_fd` in destructor |
| `OpenMode` | enum | Bit-flag set: `ReadOnly`, `WriteOnly`, `ReadWrite`, `Append`, `Create`, `Truncate`, `Memory` |
| `open_file(path, mode)` | function | Open a UTF-8 path with `OpenMode` flags; returns invalid `OsFd` on error |
| `open_memfd(name)` | function | Anonymous in-memory fd (`memfd_create` on Linux; temp-file fallback elsewhere) |
| `read(fd, buf, size)` | function | Read up to `size` bytes; returns bytes read or -1 on error |
| `write(fd, buf, size)` | function | Write `size` bytes; returns bytes written or -1 on error |
| `seek(fd, offset, whence)` | function | POSIX-semantics seek (SEEK_SET/CUR/END); returns new offset or -1 |
| `tell(fd)` | function | Current file offset; returns -1 on error |
| `flush(fd)` | function | Flush OS write buffers (`fsync` / `FlushFileBuffers` — Win32 avoids `_commit`, which asserts on read-only fds in the debug CRT) |
| `close_fd(raw_fd)` | function | Close a raw int fd; used by `OsFd::close()` |
| `file_size(path)` | function | File size in bytes, or `nullopt` on failure |
| `file_time(path)` | function | Last-write timestamp (`file_time_type`), or `nullopt` on failure |
| `list_directory(path)` | function | Names (not paths) of all entries under `path`; empty on error |
| `is_case_insensitive(path)` | function | True if the directory's volume is case-insensitive natively |
| `make_directory(path)` | function | Create directory; true if created or already exists |
| `rename_file(from, to)` | function | Rename or move file; returns success flag |
| `delete_file(path)` | function | Delete file; returns success flag |

#### Android AAsset bridge (XASH_ANDROID builds only)

| Symbol | Kind | Description |
|--------|------|-------------|
| `AssetManagerHandle` | struct | Opaque handle wrapping `AAssetManager` |
| `android_init_jni(env, activity, cls)` | function | Must be called once at `JNI_OnLoad` before any asset ops |
| `get_asset_manager(engine_package)` | function | Obtain `AssetManagerHandle` via JNI; returns nullptr before init |
| `list_assets(mgr, path)` | function | Names under `path` in the APK assets tree |
| `asset_exists(mgr, path)` | function | True if asset exists in the APK |
| `open_asset(mgr, path)` | function | Copy asset into an anonymous in-memory `OsFd` at position 0 |

### Console I/O (`xash::platform::console`, `include/xash3dpp/platform/console.hpp`)

| Symbol | Kind | Description |
|--------|------|-------------|
| `console::write(text)` | function | Write a UTF-8 string to the system console; never blocks |
| `console::read_line()` | function | Poll for a complete input line (no newline); returns `{}` if none ready; view is valid until the next call |

### Crash handling (`xash::platform::crash`, `include/xash3dpp/platform/crash.hpp`)

| Symbol | Kind | Description |
|--------|------|-------------|
| `crash::install_handler()` | function | Register signal/SEH crash handler; idempotent; must be called from the main thread before any other threads |
| `crash::print_trace()` | function | Write best-effort stack trace to stderr/logcat; async-signal-safe; no heap allocation |

### Socket I/O (`xash::platform`, `include/xash3dpp/platform/os_socket.hpp` + `platform_sockets.hpp`)

All functions are annotated `// @thread-safety: T_NetIO-ready` — callable from
`ThreadRole::Main` today; callable from a future `ThreadRole::NetIO` thread
with no code change (threading-model.md §7).

| Symbol | Kind | Description |
|--------|------|-------------|
| `OsSocket` | class | RAII wrapper around a native socket handle (`SocketHandle`). Non-copyable; movable. `close()` defined in the platform TU. |
| `SocketHandle` | type alias | `int` (POSIX) or `uintptr_t` (Win32, matches `SOCKET`). |
| `k_invalid_socket` | constant | Platform sentinel for an invalid/closed handle. |
| `IpFamily` | enum | `V4`, `V6` (re-exported from `networking/address.hpp`). |
| `NetAddress` | struct | IPv4/IPv6 + port endpoint (re-exported from `networking/address.hpp`). |
| `socket_init()` | function | Win32: `WSAStartup` ref-counted; POSIX: no-op. |
| `socket_shutdown()` | function | Win32: `WSACleanup` ref-counted; POSIX: no-op. |
| `open_udp_socket(family, port, bind_iface)` | function | Create + bind a non-blocking UDP socket; `port==0` → ephemeral. Returns `Result<OsSocket>`. |
| `open_tcp_socket(family)` | function | Create a non-blocking TCP socket (no bind). Returns `Result<OsSocket>`. |
| `set_non_blocking(sock, on)` | function | Toggle non-blocking mode (`FIONBIO` / `fcntl O_NONBLOCK`). |
| `set_broadcast(sock, on)` | function | `SO_BROADCAST` for master-server discovery. |
| `set_reuse_addr(sock, on)` | function | `SO_REUSEADDR` for dedicated-server restart. |
| `set_recv_buffer(sock, bytes)` | function | `SO_RCVBUF`. |
| `set_send_buffer(sock, bytes)` | function | `SO_SNDBUF`. |
| `bind_socket(sock, address)` | function | Bind a socket not bound at creation. |
| `sendto(sock, data, to)` | function | UDP send. Returns `Result<size_t>`. |
| `recvfrom(sock, buffer, from_out)` | function | UDP receive. Returns `NetError::WouldBlock` when no data queued. |
| `send_stream(sock, data)` | function | TCP write; partial writes reported by byte count. |
| `recv_stream(sock, buffer)` | function | TCP read; 0 bytes = orderly shutdown. |
| `connect_stream(sock, to)` | function | Non-blocking connect; `NetError::WouldBlock` = "in progress". |
| `get_local_address(sock)` | function | Bound local `NetAddress`, or `nullopt`. |
| `resolve_blocking(host, family)` | function | Synchronous `getaddrinfo`. **Worker/NetIO ONLY** — must not be called from `ThreadRole::Main`. |
| `IPlatformSockets` | struct (abstract) | Injectable seam for the networking subsystem hot path. |
| `default_platform_sockets()` | function | Returns the process-singleton production `IPlatformSockets`. |

`Result<T>` is `std::expected<T, xash::networking::NetError>` (C++23,
introduced with this surface).

**Error mapping** (full table in `docs/architecture/platform/sockets.md`):

| OS error | `NetError` |
|----------|------------|
| `EAGAIN`/`EWOULDBLOCK`/`WSAEWOULDBLOCK` | `WouldBlock` |
| `EBADF`/`WSAENOTSOCK` | `SocketInvalid` |
| `EADDRINUSE`/`WSAEADDRINUSE` | `BindFailed` |
| `EMSGSIZE`/`WSAEMSGSIZE` | `Overflow` |
| `EINVAL` from `bind` | `BadAddress` |
| Receive buffer truncated | `BufferTooSmall` |
| Pre-`WSAStartup` call | `NotInitialised` |
| `EAI_AGAIN` | `DnsAgain` |
| Other `getaddrinfo` failure | `DnsFailure` |

## Dependencies (what this module calls)

| Subsystem | Why |
|-----------|-----|
| `xash3dpp_utilities` (PRIVATE) | `path::extract_dir`, `path::fix_slashes` for normalising paths returned by `get_executable_dir` |
| OS libraries | `kernel32` (Win32, implicit); `Ws2_32` (Win32, sockets); `dl` (`-ldl`, POSIX) for dlopen/dlsym/dlclose; `android` + `log` (Android NDK) |
| `networking/errors.hpp`, `networking/address.hpp` | Header-only types — `NetError`, `Result<T>`, `NetAddress`, `IpFamily` — used by the socket API surface. No link-time dependency on `xash3dpp_networking`. |

No dependency on `xash3dpp_memory` — all public functions return by value
(`std::string`, `std::vector`, `OsFd`, `OsSocket`, `LibHandle`) or operate on
primitive types. No pool-backed long-lived state is needed.

## Owned state

| State | Location | Notes |
|-------|----------|-------|
| Clock epoch | `static const double s_epoch` in `get_time()` | Initialised once on first call via C++11 magic-static; thread-safe |
| (Win32) QPC frequency and start | `static const auto` in `get_time()` | Same magic-static pattern |
| Console input line buffer | `static char` array in `read_line()` | Main-thread only; overwritten on each call |
| Crash handler flag | `static bool` in `install_handler()` | Set once; prevents double-registration |
| (Android) JNI state | Statics in `android_init_jni()` | Written once at `JNI_OnLoad`; read-only thereafter |
| (Win32) WSA refcount | `static std::atomic<int> s_wsa_refcount` in `win32/os_socket.cpp` | Incremented by `socket_init()`, decremented by `socket_shutdown()`. `WSAStartup`/`WSACleanup` called on transitions 0→1 and 1→0. |

No `Init` / `Shutdown` functions. The subsystem is ready to use immediately
without any explicit initialisation, except that `install_handler()` should be
called once from the main thread at startup. On Win32, `socket_init()` must be
called before any socket functions and `socket_shutdown()` called at teardown;

## Quirks and invariants

- `get_time()` returns **elapsed seconds from first call**, not wall-clock time.
  The epoch resets if the process image is replaced (irrelevant in practice).
- `open_library` on Win32 converts the UTF-8 path to UTF-16 before calling
  `LoadLibraryW`. Raw `LoadLibraryA` is never used.
- `get_executable_dir()` returns the **directory**, not the full path to the
  executable. The result always ends with a forward slash and never contains
  backslashes.
- `close_library` zeroes the `LibHandle::native` field on success. Calling it
  on an already-null handle is a no-op.
- `message_box` on POSIX writes to `stderr`. Builds that include SDL2 can
  override this behaviour at the renderer/host layer.
- `shell_execute` uses `fork` + `execvp` on POSIX (fire-and-forget); if
  `fork` fails the call is silently dropped. No guarantee of success on any
  platform.
- `is_debugger_present` reads `/proc/self/status` on Linux; returns `false`
  on macOS, BSD, and all embedded targets (TODO: implement per-platform).
- `open_file` on Win32 converts the UTF-8 path to UTF-16 before calling
  `_wopen`. Raw `_open` on a `const char *` path is never used.
- `console::read_line()` returns a `string_view` into a static buffer. Callers
  must copy the result before the next call or frame boundary.
- `crash::install_handler()` is idempotent. It is safe to call at `main()`
  entry before any threads are spawned; calling it from a non-main thread is
  undefined behaviour on POSIX (signal disposition is process-wide but
  `sigaction` must be called before threads that may catch signals).
- `open_udp_socket` / `open_tcp_socket` return sockets already in non-blocking
  mode. `set_non_blocking` exists only for handles obtained externally.
- `socket_init` / `socket_shutdown` are ref-counted and may be called multiple
  times (e.g. by different subsystems); the WSAStartup/Cleanup pair fires only
  on the transitions 0→1 and 1→0. POSIX implementations are no-ops.
- `resolve_blocking` must not be called from `ThreadRole::Main`. It is
  synchronous and may block for hundreds of milliseconds. It is intended for
  the networking DNS worker (today `ThreadRole::Worker`, future `T_NetIO`).
- IPv4 / IPv6 dual-stack is NOT implemented via `IPV6_V6ONLY=0`. The networking
  subsystem holds two separate socket handles per logical endpoint (one V4, one
  V6) and round-robins between them. This matches legacy behaviour exactly.
- `NetAddress::ip6_0[0..1]` must be zero for V4 addresses. The `to_sockaddr_v4`
  conversion helpers assert this invariant with `XASH_ASSERT`.

## Open questions

- **Clipboard** (`Sys_GetClipboardData`) — needed by the in-game console for
  paste. Include here or own it in a future `xash3dpp_window` subsystem?
- **SIGTERM handling** (`Posix_SetupSigtermHandling`) — should SIGTERM be caught
  here and turned into a `host::request_quit()` call, or owned by the host?
- **Android extras** (`Android_GetKeyboardHeight`, `Android_GetAndroidID`) —
  defer until the Android build target is added.
- **High-resolution sleep** (`Win32_NanoSleep`, `SDLash_NanoSleep`) — needed by
  the frame-timing loop; add to the platform API when the host subsystem is
  started.

### Resolved

- ~~**System console / stdin** — interactive console I/O might belong in a
  separate module~~ → Implemented as `console::write` / `console::read_line` in
  `include/xash3dpp/platform/console.hpp` and the platform source files.
- ~~**Signal / exception crash handler** — should this live in platform?~~ →
  Implemented as `crash::install_handler` / `crash::print_trace` in
  `include/xash3dpp/platform/crash.hpp`.
- ~~**Raw socket API** — should low-level socket ops (UDP, TCP, name resolution)
  live in platform?~~ → Implemented as free functions + `OsSocket` RAII type
  in `include/xash3dpp/platform/os_socket.hpp`; the injectable seam
  `IPlatformSockets` is in `platform_sockets.hpp`.

______________________________________________________________________

## Extension axes (Q-21)

> Added 2026-07-06 (as-built pass). Evaluated against
> `docs/design/extension-goals.md`.

Platform is the **OS-primitive floor** the extension goals stand on: it owns
the timing, sleep, dynamic-library, socket, console and (hosted here)
thread-role primitives that every off-main thread in the north-star design
(G-1 MCP listener, G-2 threaded game ABI, G-3 debug thread, plus the
threading-model's NetIO/Worker roles) will use. It exposes almost no mutable
state of its own, so its Q-21 posture is about **keeping the OS-primitive doors
open**, not about de-globalising a legacy core.

| Goal / primitive | Applies? | Required seam or door (verdict) |
|------------------|----------|--------------------------------|
| **P-1** main-thread inbox + worker pool | Enabler (door) | Platform owns no inbox, but every off-main thread needs an OS **thread-spawn + `ThreadRole`-registration** primitive. `core/thread_role.cpp` is already hosted in this target; there is **no** `platform::spawn_thread` yet (threads forbidden until P-1 lands — OQ-9 posture). **Door-keep:** add a thin thread-spawn wrapper that registers a `ThreadRole` on entry when the first off-main consumer (Chunk 7 worker pool) is scheduled — do not build it early |
| **P-2** published-snapshot reads | No | Platform holds no sim state; nothing to snapshot |
| **P-3** context-first, no new file-scope state | **Yes** | Every entry point is already a free function over a caller-owned handle or a pure OS query. The **only** file-scope mutable state is the documented exception set (magic-static clock epoch, WSA refcount atom, crash-installed flag, Android JNI glue). **Door-keep:** no new statics; a v2 thread-spawn primitive must take a `ThreadRole` argument, not read a global |
| **P-4** typed introspection | Minor | Stateless ⇒ little to introspect. A future `platform_stats` (open socket / open library counts) is the only P-4 tier; low priority. No `extern` poke risk today |
| **P-6** services are satellites | N/A (floor) | Platform is the mandatory floor, **not** a satellite (Q-11 score 0 — one impl per OS, compile-time selected). The one seam, `IPlatformSockets`, is a Q-7 test seam, not a satellite feature |
| **P-7** over-aligned pool alloc | No | Platform performs no allocation (all returns are by-value `std::string`/`std::vector`/RAII handles) |
| **G-1** in-engine MCP service | Door-keep | Transport primitives already live here: `open_tcp_socket` + `IPlatformSockets` (TCP/WebSocket transport) and `console::write`/`read_line` (stdio transport). The socket layer is already `T_NetIO-ready`. **Door:** the listener thread rides on the same P-1 thread-spawn primitive above |
| **G-2** game ABI v2 | Door-keep | `open_library`/`get_symbol`/`close_library` load the versioned plugin descriptor (Q-10) and the future v2 game DLL. **Door-keep:** keep dynlib reentrant and context-free (it is) so a v2 loader can run off-main |
| **G-3** dedicated debug thread | Door-keep | Reads via the already-atomic stats and the hosted `assert_thread_role` enforcement; the thread itself needs the P-1 spawn primitive + a new `ThreadRole` enum value (additive). Nothing in platform blocks it |
| **G-5** scripting runtime | Door-keep | `open_library` loads the isolated-island script satellite (extension-goals §G-5 names "platform dynlib" as an existing affordance). No exception/RTTI leak risk — platform is `/EHs-c- /GR-` like every engine target |
| **NetIO / DNS** (threading-model §7) | **Yes — headline** | `resolve_blocking` is contractually **Worker/NetIO-only** (synchronous `getaddrinfo`, may block 100s of ms); the socket setters + send/recv are `T_NetIO-ready`. This is the clearest already-open off-main door in the subsystem |

**Headline door:** the missing **OS thread-spawn + `ThreadRole`-registration
primitive**. Every off-main thread the north star names (G-1/G-3/NetIO/Worker)
needs it, and platform is its natural owner because it already hosts
`thread_role.cpp`. Today the door is kept open passively (all socket/time/
console primitives are thread-agnostic, `resolve_blocking` is Worker-only); the
active step is to add the wrapper — not before Chunk 7 schedules the first
consumer (no gold-plating, per extension-goals §3).

______________________________________________________________________

## Threading

> Refreshed 2026-07-06 (as-built pass). Enumerated every `assert_main_thread` /
> `compliance-allow(thread-assert)` / mutable-global adjudication site across
> the 14 TUs; the class table below is the analyse-threading view.

Platform is thread-agnostic by construction: every entry point is a stateless
syscall wrapper over a caller-owned handle (`OsFd`, `OsSocket`, `LibHandle`)
or a pure OS query — safe from any thread; concurrent operations on the SAME
handle are the owner's responsibility. Per QN, every public header carries a
`@thread-safety:` contract line.

### Thread-role class table (as-built)

| Class | Sites (per OS unless noted) | Posture |
|-------|-----------------------------|---------|
| **Main-thread-asserting** | `console::read_line` (win32 + posix), `crash::install_handler` (win32 + posix) | Opens with `::xash::core::detail::assert_main_thread(...)` — static line buffer / process-wide signal disposition |
| **Main-thread-capturing** | `get_time()` (win32 + posix) | First call runs `capture_main_thread()` inside the magic-static clock init — establishes the Main identity the asserts check |
| **Adjudicated non-asserting mutators** | `flush(OsFd&)`, `set_non_blocking`, `set_broadcast`, `set_reuse_addr`, `set_recv_buffer`, `set_send_buffer` (win32 + posix — 6 × 2 = 12 sites) | `compliance-allow(thread-assert)` — stateless OS-handle wrappers; a Main assert would be false precision on a `T_NetIO`-ready surface |
| **Worker/NetIO-only** | `resolve_blocking` (win32 + posix) | Synchronous `getaddrinfo`; **must not** run on `ThreadRole::Main` |
| **`T_NetIO`-ready any-thread** | all other `os_socket` free functions (`open_udp_socket`, `sendto`, `recvfrom`, `send_stream`, `recv_stream`, `connect_stream`, …) | Header-annotated `@thread-safety: T_NetIO-ready`; callable from Main today, NetIO tomorrow, no code change |
| **Any-thread stateless** | `sleep`, `open_library`/`get_symbol`/`close_library`, `open_file`/`read`/`write`/`seek`/`tell`, `file_size`/`file_time`/`list_directory`, `message_box`, `shell_execute`, `console::write` | Pure syscall wrappers; no shared mutable state |
| **Mutable-global adjudications** | WSA refcount `std::atomic<int>` (win32 `os_socket.cpp`); crash-installed `static bool`/atomic (win32 + posix + android `crash.cpp`); Android JNI glue `g_jni` / `g_handles[2]` / `g_jni_flag` / `g_init_flags[2]` (android `os_io.cpp`) | Atomics for the refcount/flag; JNI glue is `compliance-allow(mutable-global, di-global-ref)` — bound once at `JNI_OnLoad` via `call_once` before any engine context exists, read-only thereafter |

The **diagnostics tier is hosted in this target** (`core/log.cpp`,
`core/thread_role.cpp`) so the base layer is self-contained — platform's
console/crash/socket asserts call `assert_thread_role` / `assert_main_thread`
without a core → platform link cycle (CMakeLists D-1 note; see
`docs/design/layer-model.md`). Extension-goals G-3 leans on exactly this: the
debug thread's off-main reads are gated by the same `assert_thread_role`
enforcement platform already exercises.

- **Main-thread-only surfaces assert at debug time**: `console::read_line()`
  (static line buffers) and `crash::install_handler()` (process-wide signal
  disposition must precede thread spawning) open with `assert_main_thread`;
  `get_time()` captures the main-thread ID on its first call (magic-static).
- **Adjudicated non-asserting mutators**: the five socket option setters
  (`set_non_blocking`, `set_broadcast`, `set_reuse_addr`, `set_recv_buffer`,
  `set_send_buffer`, POSIX + Win32) and `flush(OsFd&)` carry
  `compliance-allow(thread-assert)` — stateless OS-handle wrappers; thread
  affinity belongs to the handle owner, and a main-thread assert would be
  false precision on a `T_NetIO`-ready surface.
- **Worker-only**: `resolve_blocking()` is synchronous `getaddrinfo` — never
  from `ThreadRole::Main` (Worker today, `T_NetIO` when introduced).
- **Owned mutable state** is confined to: magic-static clock epochs, static
  console line buffers (main-thread-confined), the atomic WSA refcount, the
  atomic crash-handler-installed flag, and the Android JNI process glue —
  bound once at `JNI_OnLoad` via `call_once` and read-only thereafter,
  adjudicated `compliance-allow(mutable-global, di-global-ref)` at the
  definitions (no engine context exists at JNI-init time, so DI is
  structurally impossible there).

## Constant classification (QO)

The eight magic/shadow literal candidates flagged in this module are all
**fixed-size local scratch buffers — frozen implementation constants**, not
tunable capacities (`limits.hpp`) nor behavioural knobs (cvars):

- `char line[48]` (win32 + android `crash::print_trace`) — sized to the
  `"  [%02u] %p\n"` frame line worst case; async-signal-safe static scratch.
- `char status[4096]` (posix `is_debugger_present`) — one-shot read of
  `/proc/self/status`; the kernel emits well under 4 KiB.
- `char t[256], m[1024]` (win32 `message_box`) and `char p[1024], a[1024]`
  (win32 `shell_execute`) — truncating stack copies that null-terminate
  `string_view` arguments for the A-suffixed Win32 APIs.
- `std::uint8_t tmp[65536]` (android `open_asset`) — AAsset streaming chunk
  size. The value collisions with unrelated `XASH_LIMIT_*` defaults (65536,
  4096, 1024, 256) are coincidental, not shadowed limits.

Structural capacities used by this module already live in `limits.hpp`
(`platform_console_buffer_size`, `platform_console_event_buf`,
`platform_crash_frames_max`, `platform_path_buf_wchars`).

## Q-11 satellite verdict

Not a satellite candidate: platform is the mandatory OS-abstraction floor —
no compat variance, no policy injection, exactly one implementation per OS
selected at compile time (Q-11 score 0). The one injectable seam,
`IPlatformSockets`, exists as a Q-7 intra-process test seam for the networking
subsystem, not as a satellite feature; `DefaultPlatformSockets` is the only
production implementation.
