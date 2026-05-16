# Platform Boundary Spec

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
| `flush(fd)` | function | Flush OS write buffers (`fsync` / `_commit`) |
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
  `include/xash3dpp/platform/crash.hpp`.- ~~**Raw socket API** — should low-level socket ops (UDP, TCP, name resolution)
  live in platform?~~ → Implemented as free functions + `OsSocket` RAII type
  in `include/xash3dpp/platform/os_socket.hpp`; the injectable seam
  `IPlatformSockets` is in `platform_sockets.hpp`.