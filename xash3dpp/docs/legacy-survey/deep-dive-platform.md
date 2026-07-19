# Deep Dive: Legacy `engine/platform/` — OS Abstraction (Time, Sleep, Dynlib, Console, Crash, Dialogs)

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy per-OS abstraction layer at
the repository root (`engine/platform/**` + `engine/platform/platform.h`) that
the `xash3dpp` `platform` subsystem replaces. Wide survey coverage lives in
[engine-common-and-platform.md](engine-common-and-platform.md); this is the
narrow-and-exact companion. Line numbers are against the working tree on that
date; behaviour references, not design constraints. Everything here is
**legacy** — read it for the per-OS entry-point catalogue, the timing/sleep
semantics, and the dlopen conventions, not as a rewrite blueprint (the rewrite
chose a leaner free-function + RAII-handle design with exactly one impl per OS
selected at CMake time — see §8).*

Primary sources:

- `engine/platform/platform.h` — the `Platform_*` API + per-OS
  `Platform_Init`/`Platform_Shutdown` dispatch (static inlines)
- `engine/platform/win32/sys_win.c` — Win32 time / sleep / dialogs / debugger /
  shell-execute + `Win32_Init`/`Win32_NanoSleep`
- `engine/platform/posix/sys_posix.c` — POSIX shell-execute / daemonize / SIGTERM
- `engine/platform/win32/lib_win.c` + `lib_win.h` — Win32 dynamic-library loader
  (incl. the custom in-memory PE loader)
- `engine/platform/posix/lib_posix.c` — POSIX `dlopen` loader + embedded-target
  compat shims
- `engine/platform/win32/con_win.c` (`Wcon_*`) / `engine/platform/posix/con_posix.c`
  (`Posix_Input`) — system console I/O
- `engine/platform/win32/crash_win.c` (SEH) /
  `engine/platform/posix/crash_posix.c` + `crash_libbacktrace.c` — crash handling
- `engine/platform/{win32,posix}/net.h` — the WinSock↔BSD socket typedef wrap
  (the socket *code* lives in `engine/common/net_ws.c`, not here)
- `engine/platform/misc/` — `kmalloc.c`, `lib_static.c`, `sbrk.c`, `swap.h`

**Global assumptions:** the legacy layer is a **macro-selected `#ifdef` mesh**,
not an interface. Backend choice is compile-time via `XASH_TIMER`, `XASH_LIB`,
`XASH_MESSAGEBOX`, `XASH_INPUT`, `XASH_CON` and the `XASH_<OS>` family; a single
build may mix (e.g. Android = POSIX `sys` + SDL input + Android console). All
state is process-global; there is no context object. Subdirectories present:
`win32/`, `posix/`, `sdl1/`, `sdl2/`, `sdl3/`, `android/`, `ios/`, `nswitch/`,
`psvita/`, `dos/`, `irix/`, `linux/`, `misc/`, `stub/`.

______________________________________________________________________

## 1. The `Platform_*` API surface (`platform.h`)

The contract every OS backend fills:

| Function | Semantics |
|----------|-----------|
| `double Platform_DoubleTime( void )` | Monotonic elapsed seconds; epoch = first call (see §2) |
| `void Platform_Sleep( int msec )` | Millisecond thread sleep |
| `void Platform_ShellExecute( const char *path, const char *parms )` | Open path with OS default handler (§5) |
| `void Platform_MessageBox( const char *title, const char *message, qboolean parentMainWindow )` | Modal error dialog (`XASH_MESSAGEBOX` backend) |
| `void Platform_SetStatus( const char *status )` | Console/title status line (Win32 + Linux only — `XASH_PLATFORM_HAVE_STATUS`) |
| `qboolean Platform_DebuggerPresent( void )` | Debugger attached? (§6) |
| `platform_orientation_t Platform_GetDisplayOrientation( void )` | Mobile screen orientation |

**Per-OS lifecycle** is dispatched by two `static inline` shims in `platform.h`
that fan out on `XASH_<OS>`:

- `Platform_Init( qboolean con_showalways )` → `Posix_Daemonize` (POSIX, first),
  `SDLash_Init` (if `XASH_SDL`), then exactly one of `Android_Init` /
  `NSwitch_Init` / `PSVita_Init` / `DOS_Init` / `Win32_Init` / `Linux_Init`.
- `Platform_Shutdown()` → the matching `*_Shutdown` + `SDLash_Shutdown`.
- `Platform_NanoSleep( int nsec )` → `SDLash_NanoSleep` (SDL3), else the POSIX
  `nanosleep` path, else `Win32_NanoSleep` (§3).
- `Platform_SetupSigtermHandling()` → `Posix_SetupSigtermHandling` (§5).

Conditional per-target extras (all `#if XASH_<OS>`): `Posix_Input`,
`Win32_NanoSleep` + the `Wcon_*` console family, `Android_Get*`/`Android_Save*`,
`Linux_SetTimer`/`Linux_GetProcessID`, `PSVita_GetBasePath`/`PSVita_GetArgv`,
`IOS_GetArgs`/`IOS_GetDocsDir`.

______________________________________________________________________

## 2. Timing — `Platform_DoubleTime` (`sys_win.c:23`, `XASH_TIMER`)

Selected by `XASH_TIMER` (`TIMER_WIN32`, SDL, POSIX, …). The Win32 backend:

```c
double Platform_DoubleTime( void )
{
    static LARGE_INTEGER g_PerformanceFrequency;   // magic-static, first-call init
    static LARGE_INTEGER g_ClockStart;
    LARGE_INTEGER CurrentTime;
    if( !g_PerformanceFrequency.QuadPart ) {
        QueryPerformanceFrequency( &g_PerformanceFrequency );
        QueryPerformanceCounter( &g_ClockStart );
    }
    QueryPerformanceCounter( &CurrentTime );
    return (double)( CurrentTime.QuadPart - g_ClockStart.QuadPart )
         / (double)( g_PerformanceFrequency.QuadPart );
}
```

Quirks: the epoch is **the first call**, not process start or wall-clock; the
first-call lazy init is a non-thread-safe `if( !freq )` guard (the rewrite uses
a C++ magic-static). POSIX uses `clock_gettime( CLOCK_MONOTONIC )`. `Sleep(msec)`
(Win32) / `nanosleep` (POSIX) provide the coarse sleep.

______________________________________________________________________

## 3. High-resolution sleep — `Win32_NanoSleep` (`sys_win.c:46,84`)

Legacy has a **sub-millisecond** sleep the frame-timing loop uses, absent from
the rewrite (open question / modernization L-3). `Win32_Init` probes
`kernel32!CreateWaitableTimerExW` by `GetProcAddress` and creates a
`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION | CREATE_WAITABLE_TIMER_MANUAL_RESET`
timer into the file-global `g_waitable_timer`:

```c
qboolean Win32_NanoSleep( int nsec )
{
    LARGE_INTEGER ts;
    if( !g_waitable_timer ) return false;
    ts.QuadPart = -nsec / 100;                 // 100 ns units, negative = relative
    if( !SetWaitableTimer( g_waitable_timer, &ts, 0, NULL, NULL, FALSE )) { ... return false; }
    if( WaitForSingleObject( g_waitable_timer, Q_max( 1, nsec / 1000000 )) != WAIT_OBJECT_0 )
        return false;
    return true;
}
```

`SDLash_NanoSleep` (SDL3) and the raw POSIX `nanosleep` cover the other targets.

______________________________________________________________________

## 4. Dynamic-library loading

### 4.1 POSIX (`lib_posix.c`, `XASH_LIB == LIB_POSIX`)

Straight `dlfcn.h`: `dlopen(path, RTLD_NOW | RTLD_LOCAL)` → `dlsym` → `dlclose`,
with per-embedded-target compat: NSwitch `solder.h` (`SOLDER_LIBDL_COMPAT`),
PSVita `vrtld.h` (`VRTLD_LIBDL_COMPAT`), IRIX a hand-rolled `dladdr` shim, and
an `XASH_NO_LIBDL` fully-static fallback. It also drags in `filesystem.h`,
`server.h` and `platform/android/lib_android.h` — the loader is **not** cleanly
separated from the engine (the rewrite isolates it to `sys.cpp`).

### 4.2 Win32 (`lib_win.c` + `lib_win.h`, `XASH_LIB == LIB_WIN32`)

Two loaders coexist:

1. **Standard** — `LoadLibraryW` via a truncating `static wchar_t
   pathBuffer[MAX_PATH]` conversion (`FS_PathToWideChar`, one-call
   `MultiByteToWideChar(..., -1, ..., MAX_PATH)`).
2. **Custom in-memory PE loader** — `COM_LoadLibrary` can map a DLL that lives
   *inside* a PAK/archive by hand: it walks the PE headers (`GetOffsetByRVA`
   over `IMAGE_NT_HEADERS` / `IMAGE_SECTION_HEADER`), reads the export table,
   and resolves names with `FsGetString`/`FS_Getc`. This exists so mods can ship
   DLLs packed in archives.

The rewrite drops the custom in-memory loader entirely (only `LoadLibraryW` /
`dlopen`) and replaces the truncating `MAX_PATH` conversion with the two-call
pattern — the surviving inconsistency is tracked as platform modernization H-1.

______________________________________________________________________

## 5. Shell-execute, daemonize, SIGTERM (`sys_posix.c`)

- **`Platform_ShellExecute`** — Win32 `ShellExecuteA(NULL,"open",path,parms,...)`
  (`sys_win.c`); POSIX `fork()` + `execvp( OPEN_COMMAND, {OPEN_COMMAND, path} )`
  (`OPEN_COMMAND` = `xdg-open` on Linux). The POSIX child is **never `wait`ed**
  — the zombie the rewrite's L-2 TODO calls out.
- **`Posix_Daemonize`** (`-daemonize`) — `fork` → `setsid` → `umask(0)` →
  `close(0/1/2)` and reopen `/dev/null`. Mobile targets `Sys_Error` (unsupported).
- **`Posix_SetupSigtermHandling`** — installs `Posix_SigtermCallback`, which
  formats `"caught signal %d"` and calls `Sys_Quit`. The rewrite's boundary spec
  lists SIGTERM→`host::request_quit()` as an open question (host vs platform
  ownership).

______________________________________________________________________

## 6. Debugger detection & console I/O

- **`Platform_DebuggerPresent`** — Win32 `IsDebuggerPresent()`; Linux parses
  `TracerPid:` out of `/proc/self/status`. Only Linux/Win32 are wired
  (`Sys_DebuggerPresent` returns `false` elsewhere) — mirrored by the rewrite's
  macOS/BSD stub TODO (modernization L-1).
- **Console** — Win32 owns a full allocated console via the `Wcon_*` family
  (`Wcon_CreateConsole`, `Wcon_ShowConsole`, `Wcon_Input`, `Wcon_WinPrint`) in
  `con_win.c`; POSIX polls stdin via `Posix_Input` in `con_posix.c`. The rewrite
  collapses this to `console::write` / `console::read_line`.
- **Crash** — Win32 SEH handler in `crash_win.c`; POSIX `sigaction` handlers in
  `crash_posix.c`, with an optional `libbacktrace` symboliser
  (`crash_libbacktrace.c`). The rewrite exposes `crash::install_handler` /
  `crash::print_trace` (async-signal-safe, no heap).

______________________________________________________________________

## 7. Sockets — where they actually live

There is **no** socket abstraction under `engine/platform/` proper: the per-OS
`net.h` files only paper over WinSock↔BSD naming (`platform/win32/net.h`:
`#include <ws2tcpip.h>` + `typedef int WSAsize_t`). The real socket code
(`WSAStartup`, `socket`, `bind`, `sendto`/`recvfrom`, `getaddrinfo`,
non-blocking setup, dual-stack V4/V6) lives in `engine/common/net_ws.c`. The
rewrite **relocated** raw socket ops into `xash3dpp_platform`
(`os_socket.hpp` + per-OS `os_socket.cpp` + the `IPlatformSockets` seam), so the
mapping below records `net_ws.c` as the legacy source for that surface.

______________________________________________________________________

## 8. As-built mapping (`engine/platform/` legacy → `xash3dpp/…/platform`)

| Legacy construct | Site | `xash3dpp` as-built | Notes |
|------------------|------|---------------------|-------|
| `Platform_*` API + `#ifdef` mesh | `platform.h` | Free functions in `xash::platform`; **one impl per OS** picked in `src/platform/CMakeLists.txt` (win32 / posix / android subdirs) | No macro backend switch; header carries `@thread-safety:` per QN |
| `Platform_DoubleTime` (`if(!freq)` lazy init) | `sys_win.c:23` | `get_time()` with a C++ magic-static clock epoch + `capture_main_thread()` | Thread-safe first-call init |
| `Platform_Sleep` / `Win32_NanoSleep` / `SDLash_NanoSleep` | `sys_win.c` | `sleep(ms)` only | Sub-ms `sleep_precise` deferred (modernization L-3) |
| `COM_LoadLibrary` custom in-memory PE loader | `lib_win.c` | **Eliminated** — `open_library` = `LoadLibraryW` only | Packed-DLL loading dropped |
| `FS_PathToWideChar` (`static wchar_t[MAX_PATH]`, truncating) | `lib_win.c:22` | `open_library` UTF-16 convert (still fixed-buffer — modernization **H-1**); `os_io.cpp` `to_wide` = two-call `std::wstring` | H-1 completes the H-2-style fix |
| `dlopen`/`dlsym`/`dlclose` (+ solder/vrtld/dladdr shims) | `lib_posix.c` | `open_library`/`get_symbol`/`close_library` (`RTLD_NOW\|RTLD_LOCAL`) via `LibHandle` RAII | Embedded-target shims dropped for now |
| `Wcon_*` console family / `Posix_Input` | `con_win.c` / `con_posix.c` | `console::write` / `console::read_line` (`main`-thread asserting) | `include/xash3dpp/platform/console.hpp` |
| SEH / `sigaction` handlers + `libbacktrace` | `crash_win.c` / `crash_posix.c` / `crash_libbacktrace.c` | `crash::install_handler` / `crash::print_trace` (idempotent, async-signal-safe) | `crash.hpp`; installed flag = atomic |
| `Platform_ShellExecute` (ShellExecuteA / fork+execvp) | `sys_win.c` / `sys_posix.c` | `shell_execute(path, params)` | POSIX zombie reaping = modernization **L-2** |
| `Platform_MessageBox` (`XASH_MESSAGEBOX`) | `sys_win.c` | `message_box(title, msg)` (stderr fallback headless) | Truncating `char t[256]/m[1024]` stack copies (frozen scratch) |
| `Platform_DebuggerPresent` (IsDebuggerPresent / `/proc`) | `sys_win.c` / `sys_posix.c` | `is_debugger_present()` | macOS/BSD stub = modernization **L-1** |
| `Posix_Daemonize` / `Posix_SetupSigtermHandling` | `sys_posix.c` | **Not ported** — daemonize is host policy; SIGTERM is a boundary open question | Host vs platform ownership |
| all OS **file I/O** (`open`/`read`/`seek`/`stat`, dir ops, UTF-16) | `filesystem.c`/`dir.c` + win32 inline | `os_io.hpp` + per-OS `os_io.cpp` (`OsFd` RAII, `OpenMode`) | Extracted into platform; carries relocated fs **H-2/L-4** |
| socket API (`WSAStartup`, `socket`, `sendto`, `getaddrinfo`, V4/V6) | `engine/common/net_ws.c` (+ `platform/*/net.h` wrap) | `os_socket.hpp` + per-OS `os_socket.cpp` + `IPlatformSockets` seam; `std::expected<T,NetError>` | `resolve_blocking` Worker/NetIO-only; WSA refcount atomic |
| Android JNI / `AAssetManager` glue | `platform/android/*` + `filesystem/android.c` | `android/os_io.cpp` AAsset bridge + `android_init_jni` (`call_once`) | `compliance-allow(mutable-global)` — bound at `JNI_OnLoad` |
| `misc/kmalloc.c`, `sbrk.c`, `lib_static.c`, `swap.h` | `platform/misc/` | Not ported (byte-swap → `xash3dpp_utilities/swap`; static-link glue N/A) | See deep-dive-utilities / deep-dive-memory |

The diagnostics tier (`core/log.cpp`, `core/thread_role.cpp`) is **built into
the `xash3dpp_platform` target** (not a legacy-platform concern) so the base
layer is self-contained and the historical core ⇄ platform link cycle is broken
(CMakeLists D-1 note; boundary spec §Threading).
