# Platform Boundary Spec

> Refreshed 2026-07-06 (as-built pass). Re-scanned all 14 TUs under
> `src/platform/{win32,posix,android}/**` and the public headers under
> `include/xash3dpp/platform/**`. `status_table.py` reports platform
> **Complete** (14 TUs, tests ✔); `compliance_scan.py platform` is **clean**
> (0 blocker/warning/note; **18** documented `compliance-allow` adjudications
> as of the S9.0 pass — 12 thread-assert sites across the posix/win32
> `os_io`/`os_socket` TUs (the "6 × 2 = 12" of the Threading table), 2 more
> thread-assert sites on `spawn_thread()` itself (win32/posix `thread.cpp`,
> S9.0 — see Threading below), plus 4 mutable-global exception sites);
> `stub_scan.py platform` shows **2** TODO
> markers (both in `posix/sys.cpp`: `is_debugger_present` macOS/BSD, and a
> `shell_execute` double-fork note). Drift found this pass: the OS **file I/O**
> backend (`os_io.hpp` + per-OS `os_io.cpp`) is now owned here — it absorbed the
> filesystem **H-2 / L-4** items during the `src/filesystem/platform/{win32,
> posix}.cpp` extraction (see `modernization-opportunities/platform-modernization.md`).
> The socket surface (`os_socket` + `IPlatformSockets`) and the hosted
> diagnostics tier (`core/log.cpp`, `core/thread_role.cpp` built into this
> target) are reflected below. New sections added this pass:
> **Extension axes (Q-21)**; the **Threading** section gained a class table.
>
> **2026-07-19 addendum (SAV-OQ-3):** `name_for_symbol()` / `enumerate_exports()`
> landed in `win32/sys.cpp` + `posix/sys.cpp` — the dynlib reverse-lookup
> primitive (address → exported name) `save-boundary.md`'s SAV-OQ-3 recommended
> shape asked platform to own, for Chunk 8's `FIELD_FUNCTION` codec. Reflected
> in the Interface table, Quirks, and Threading below.
>
> **2026-07-20 addendum (S9.0, Q-24 OS-boilerplate half):** `spawn_thread()` /
> `JoinHandle` / `ThreadPriority` landed in `win32/thread.cpp` +
> `posix/thread.cpp` (new `include/xash3dpp/platform/thread.hpp`) — the
> **headline P-1 door** this doc's Extension-axes table kept open ("threads
> forbidden until P-1 lands"). Chunk 9's `T_AudioDecoder` is the first
> scheduled consumer, per `design/thread-spawn-and-inbox-brief.md` §3.3 (HB-4
> design brief). `stub_scan.py platform` now reports **3** TODO markers (the
> existing 2, plus `posix/thread.cpp`'s macOS/BSD thread-naming gap — mirrors
> the pre-existing `is_debugger_present` macOS/BSD TODO shape exactly).
> Reflected in the Interface table, Threading, and Extension axes (P-1) below.

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
| `name_for_symbol(lib, addr)` | function | Reverse lookup: exported name for an address (SAV-OQ-3). `optional<string_view>`, view lives in the module's own image — no allocation, valid until `close_library` |
| `ExportVisitor` | type alias | `void(*)(string_view name, const void *addr, void *userdata) noexcept` — callback for `enumerate_exports` |
| `enumerate_exports(lib, visit, userdata)` | function | Visit every named, non-forwarder export of `lib`. Win32: full PE export-table walk. POSIX: **unsupported**, always returns `false` (no `dladdr` enumeration primitive; matches `lib_posix.c`'s capability level) |
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

### Thread spawn (`xash::platform`, `include/xash3dpp/platform/thread.hpp`) — landed S9.0

| Symbol | Kind | Description |
|--------|------|-------------|
| `ThreadPriority` | enum class | `Normal` (default), `High` (best-effort raise), `Realtime` (`XASH3DPP-STUB(chunk12)` — logs + runs at `Normal` until the SDL audio device chunk) |
| `ThreadFn` | type alias | `void (*)(void *user) noexcept` — the C-idiom entry point (matches `cmd_cvar::CommandCtxFn`); no templated `Fn&&` |
| `JoinHandle` | class | Join-on-destruction RAII handle; movable, non-copyable; explicit `join()` and `joinable()` also exposed. Wraps `std::thread` (not `std::jthread` — no stop-token semantics needed) |
| `spawn_thread(role, name, prio, fn, user)` | function | Spawns an OS thread running `fn(user)`. `register_thread_role(role)` fires as the FIRST action on the new thread, before naming/priority/`fn`. `name` is copied into a fixed buffer (`limits::platform_thread_name_max`) at call time — does not need to outlive the call. `user` is a borrowed pointer — @lifetime: caller, must outlive the spawned thread |

Win32 names the thread via `SetThreadDescription`, resolved dynamically
(`GetProcAddress` on `kernel32.dll`) since it is a Windows 10 1607+ API and
the project pins no minimum `_WIN32_WINNT` — no-op on older Windows. POSIX
names via `pthread_setname_np` on Linux/Android (glibc/bionic truncate to 16
bytes including the null terminator); macOS/BSD naming is TODO (mirrors the
existing `is_debugger_present` macOS/BSD gap). `ThreadPriority::High` maps to
`SetThreadPriority(THREAD_PRIORITY_HIGHEST)` on Win32 and a best-effort
`SCHED_RR` bump on POSIX (silently stays at `Normal` without `CAP_SYS_NICE`
/ root — logged as a `Warning`, not a failure).

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
| OS libraries | `kernel32` (Win32, implicit); `Ws2_32` (Win32, sockets); `dl` (`-ldl`, POSIX) for dlopen/dlsym/dlclose; `android` + `log` (Android NDK); `Threads::Threads` (CMake `find_package(Threads)`, PUBLIC on `xash3dpp_platform` since S9.0 — `spawn_thread` constructs a real `std::thread`; no-op on Win32, `-lpthread` on POSIX where the libc does not fold it into libc itself) |
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
- `name_for_symbol` (SAV-OQ-3, Win32) walks the **mapped** PE export directory
  of the already-loaded module — no file re-read, unlike legacy's file-based
  `LibraryLoadSymbols` (`lib_win.c:84-304`). Only the `AddressOfNames`-sized
  arrays are walked, so ordinal-only exports (no name) never match, exactly
  like legacy. Forwarder exports (an `AddressOfFunctions` RVA landing inside
  the export directory's own address range) are explicitly detected and
  skipped — legacy's `COM_NameForFunction` (`lib_win.c:581-601`) has **no**
  such check and would treat a forwarder's RVA as a code offset; this is a
  deliberate bug-fix deviation from legacy, not an observed parity break.
- `name_for_symbol` (POSIX) uses `dladdr()` exactly like legacy's
  `COM_NameForFunction` (`lib_posix.c:153-166`), including its POSIX-only
  imprecision: `lib` is required to be non-null but is **not** cross-checked
  against the resolved object (`dlopen()`'s handle is an opaque,
  implementation-defined token — not portably comparable to `dladdr()`'s
  `dli_fbase`), so a match can in principle come from a different loaded
  module than `lib`. Same limitation as legacy, which ignores its
  `hInstance` parameter entirely on POSIX.
- `enumerate_exports` is Win32-only; POSIX always returns `false` and never
  invokes the callback (`dladdr` has no enumeration primitive, and
  `lib_posix.c` has no ordinals table at all — `COM_FunctionFromName` there
  resolves purely by name via `dlsym`).
- `message_box` on POSIX writes to `stderr`. Builds that include SDL2 can
  override this behaviour at the renderer/host layer.
- `shell_execute` uses `fork` + `execvp` on POSIX (fire-and-forget); if
  `fork` fails the call is silently dropped. No guarantee of success on any
  platform.
- `is_debugger_present` reads `/proc/self/status` on Linux; returns `false`
  on macOS, BSD, and all embedded targets (TODO: implement per-platform).
- `spawn_thread`'s debugger-visible naming step is a no-op on macOS/BSD
  (TODO: macOS's `pthread_setname_np(const char*)` takes no `pthread_t`
  (self-only) and each BSD has its own differently-signed variant) — mirrors
  the `is_debugger_present` gap immediately above rather than guessing at an
  unverified API. Linux/Android use `pthread_setname_np(pthread_self(),
  name)`, truncated to the glibc/bionic 16-byte (incl. null) hard limit.
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

## Role & parity

- **Role:** role-neutral substrate — stateless OS/syscall wrappers (files,
  sockets, threads, dlopen) under every role. No cross-role parity obligation.

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
| **P-1** main-thread inbox + worker pool | Enabler (door) — **landed S9.0** | Platform still owns no inbox (the Main-inbox drain slot stays designed-not-built per the HB-4 brief until a G-1/G-3 consumer), but the OS **thread-spawn + `ThreadRole`-registration** primitive is now built: `platform::spawn_thread(role, name, prio, fn, user) -> JoinHandle` (`win32/thread.cpp` + `posix/thread.cpp`), registering `role` via the already-hosted `core/thread_role.cpp` as the FIRST action on the new thread. Chunk 9's `T_AudioDecoder` is the first scheduled consumer (Q-24; `design/thread-spawn-and-inbox-brief.md` §3.3). The `MpscQueue`/`SpscRing` half of Q-24 lives in `core`, not here — see that register entry |
| **P-2** published-snapshot reads | No | Platform holds no sim state; nothing to snapshot |
| **P-3** context-first, no new file-scope state | **Yes** | Every entry point is already a free function over a caller-owned handle or a pure OS query. The **only** file-scope mutable state is the documented exception set (magic-static clock epoch, WSA refcount atom, crash-installed flag, Android JNI glue). **Door-keep:** no new statics; a v2 thread-spawn primitive must take a `ThreadRole` argument, not read a global |
| **P-4** typed introspection | Minor | Stateless ⇒ little to introspect. A future `platform_stats` (open socket / open library counts) is the only P-4 tier; low priority. No `extern` poke risk today |
| **P-6** services are satellites | N/A (floor) | Platform is the mandatory floor, **not** a satellite (Q-11 score 0 — one impl per OS, compile-time selected). The one seam, `IPlatformSockets`, is a Q-7 test seam, not a satellite feature |
| **P-5** narrowest-state signatures | **Already minimal** | Free-function OS wrappers take exactly the handle/value they operate on (`os_file`, socket fd, path view); no runtime aggregate exists at this layer to over-pass |
| **P-7** pool-owned RAII lifecycle | **N/A — no allocation** | Platform performs no pool allocation at all (all returns are by-value `std::string`/`std::vector`/RAII handles), so the `create_<thing>`/`pool_new` idiom has no site here. *(An earlier revision of this row answered the retired "over-aligned alloc" question — that concern lives with the HB-7 door; the answer stands: no over-alignment need.)* |
| **P-8** annotation discipline | **Yes — satisfied (denominatored)** | 2026-07-19 `annotation-coverage` scan: all marker classes at 100% for platform; the 12 `compliance-allow(thread-assert)` NetIO-ready sites + 4 mutable-global exceptions are each inline-annotated (documents-**and**-marks). |
| **G-4** expanded in-game debugging | Consumer (thin) | Overlay/console frontends consume `console::write`/`get_time` as-is; platform holds no debug state of its own beyond the (low-priority) `platform_stats` idea already noted under P-4 |
| **G-1** in-engine MCP service | Door-keep | Transport primitives already live here: `open_tcp_socket` + `IPlatformSockets` (TCP/WebSocket transport) and `console::write`/`read_line` (stdio transport). The socket layer is already `T_NetIO-ready`. **Door:** the listener thread rides on the same P-1 thread-spawn primitive above |
| **G-2** game ABI v2 | Door-keep | `open_library`/`get_symbol`/`close_library` load the versioned plugin descriptor (Q-10) and the future v2 game DLL. **Door-keep:** keep dynlib reentrant and context-free (it is) so a v2 loader can run off-main |
| **G-3** dedicated debug thread | Door-keep | Reads via the already-atomic stats and the hosted `assert_thread_role` enforcement; the thread itself needs the P-1 spawn primitive + a new `ThreadRole` enum value (additive). Nothing in platform blocks it |
| **G-5** scripting runtime | Door-keep | `open_library` loads the isolated-island script satellite (extension-goals §G-5 names "platform dynlib" as an existing affordance). No exception/RTTI leak risk — platform is `/EHs-c- /GR-` like every engine target |
| **NetIO / DNS** (threading-model §7) | **Yes — headline** | `resolve_blocking` is contractually **Worker/NetIO-only** (synchronous `getaddrinfo`, may block 100s of ms); the socket setters + send/recv are `T_NetIO-ready`. This is the clearest already-open off-main door in the subsystem |

**Headline door — landed S9.0:** the **OS thread-spawn + `ThreadRole`-
registration primitive** (`platform::spawn_thread` / `JoinHandle`). Every
off-main thread the north star names (G-1/G-3/NetIO/Worker) can now use it,
and platform was its natural owner because it already hosted
`thread_role.cpp`. The door was kept open passively until Chunk 9's
`T_AudioDecoder` became the first scheduled consumer (per extension-goals §3
"no gold-plating" — the primitive was not built early); it is now active.

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
| **Thread-spawning (role-registering)** | `spawn_thread()` (win32 + posix `thread.cpp`, landed S9.0) | The one platform entry point that CREATES a thread rather than running on an existing one. Its internal trampoline calls `register_thread_role(role)` as the FIRST action on the new thread — before naming, priority, or the caller's `fn` — so every subsequent subsystem `assert_thread_role()` call sees the correct role from the new thread's very first instruction |
| **Adjudicated non-asserting mutators** | `flush(OsFd&)`, `set_non_blocking`, `set_broadcast`, `set_reuse_addr`, `set_recv_buffer`, `set_send_buffer` (win32 + posix — 6 × 2 = 12 sites) | `compliance-allow(thread-assert)` — stateless OS-handle wrappers; a Main assert would be false precision on a `T_NetIO`-ready surface |
| **Worker/NetIO-only** | `resolve_blocking` (win32 + posix) | Synchronous `getaddrinfo`; **must not** run on `ThreadRole::Main` |
| **`T_NetIO`-ready any-thread** | all other `os_socket` free functions (`open_udp_socket`, `sendto`, `recvfrom`, `send_stream`, `recv_stream`, `connect_stream`, …) | Header-annotated `@thread-safety: T_NetIO-ready`; callable from Main today, NetIO tomorrow, no code change |
| **Any-thread stateless** | `sleep`, `open_library`/`get_symbol`/`close_library`, `name_for_symbol`/`enumerate_exports`, `open_file`/`read`/`write`/`seek`/`tell`, `file_size`/`file_time`/`list_directory`, `message_box`, `shell_execute`, `console::write` | Pure syscall wrappers; no shared mutable state |
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
- **`spawn_thread()` (S9.0)** owns no persistent state — each call's
  `ThreadStartCtx` (role, name, priority, `fn`, `user`) is a stack-local
  value, decay-copied once into `std::thread`'s own internal invoker storage
  and read exactly once by the new thread's trampoline. The role-registration
  ordering guarantee (role before `fn`) is the only cross-thread contract
  this primitive adds; it relies on `std::thread`'s constructor establishing
  a happens-before relationship with the new thread's first instruction
  (standard-guaranteed), not on any additional synchronisation here.

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
