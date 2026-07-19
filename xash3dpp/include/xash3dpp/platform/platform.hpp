#pragma once
// xash3dpp — platform abstraction: time, DLL loading, system paths, dialogs
// Legacy reference: engine/platform/win32/sys_win.c,
//                  engine/platform/posix/sys_posix.c,
//                  engine/common/system.h  (Sys_LoadLibrary, Sys_DoubleTime, …)
//
// Design notes:
//   • Free-function API only.  No class, no init/shutdown.
//   • get_time() is monotonic from first call; epoch resets across process
//     boundaries but is stable within a process lifetime.
//   • open_library / get_symbol / close_library mirror POSIX dlopen/dlsym/dlclose
//     and Win32 LoadLibraryW/GetProcAddress/FreeLibrary.
//   • All path-returning functions use forward slashes and append a trailing '/'.
//
// @thread-safety: stateless OS wrappers — safe from any thread; get_time()'s
// epoch (and main-thread capture) is a C++11 magic-static; a LibHandle value
// is caller-confined — concurrent open/close of the SAME handle is the
// owner's responsibility. name_for_symbol()/enumerate_exports() are pure
// reads over an already-loaded module image — safe for any number of
// concurrent readers as long as the module stays loaded (i.e. no concurrent
// close_library() on the same handle; that race is the existing LibHandle
// caller-confinement rule above, not new).
// Stats: no hot-path counters — stats exempt (thin OS syscall wrappers).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace xash::platform {

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

// Monotonic wall-clock time in seconds.  First call initialises the epoch.
// Subsequent calls return elapsed seconds since that first call.
// Legacy: Platform_DoubleTime → Sys_DoubleTime
[[nodiscard]] double get_time() noexcept;

// Yield the current thread for at least |ms| milliseconds.
// Passing 0 is a legal hint to the scheduler; it may return immediately.
// Legacy: Platform_Sleep
void sleep( std::uint32_t ms ) noexcept;

// ---------------------------------------------------------------------------
// Dynamic library loading
// ---------------------------------------------------------------------------

// Opaque handle to a loaded shared library.
// Default-constructed value represents "no library loaded".
struct LibHandle
{
    void *native = nullptr; // @lifetime: OS loader owns the module; released via close_library()
    explicit operator bool() const noexcept { return native != nullptr; }
};

// Load the shared library at |path| (UTF-8).  Returns {} on failure.
// Win32: converts to UTF-16 and calls LoadLibraryW.
// POSIX: calls dlopen with RTLD_NOW | RTLD_LOCAL.
// Legacy: Sys_LoadLibrary (the raw handle half; the export-table walk
//         belongs to xash3dpp_utilities dynlib helpers).
[[nodiscard]] LibHandle open_library( std::string_view path ) noexcept;

// resolve a symbol by name from a loaded library.  Returns nullptr on failure.
// Behaviour for a null LibHandle is defined: returns nullptr immediately.
// Legacy: GetProcAddress / dlsym
void *get_symbol( LibHandle lib, const char *name ) noexcept;

// Unload a shared library and zero the handle.
// Calling with a null handle is a no-op.
// Legacy: Sys_FreeLibrary
void close_library( LibHandle &lib ) noexcept;

// ---------------------------------------------------------------------------
// Dynamic library reverse lookup (SAV-OQ-3)
// ---------------------------------------------------------------------------
//
// Needed by Chunk 8's FIELD_FUNCTION save codec: legacy resolves a function
// pointer stored in a TYPEDESCRIPTION back to its exported name so the NAME
// (not the raw address) is what round-trips through the save file. Platform
// owns this primitive (a dynlib capability, per
// decisions-architecture.md SAV-OQ-3, decided 2026-07-19); save owns only
// the FIELD_FUNCTION-specific glue (the TYPEDESCRIPTION walk + a per-DLL
// ordinal cache) on top of it.
// Legacy: COM_NameForFunction (engine/platform/{win32,posix}/lib_{win,posix}.c)

// Resolve the exported name of the symbol at |addr| within |lib|.
// Returns nullopt if |lib| is null, |addr| is null, or |addr| does not match
// any named export of |lib|.
// @lifetime: the returned view points directly into the *loaded module's own
// image* — Win32: the mapped PE export-name table; POSIX: the shared
// object's own symbol-name table via dladdr(). No allocation, no copy. The
// view stays valid for as long as |lib| remains loaded (i.e. until the
// matching close_library() call) — same lifetime rule as the LibHandle
// itself. This mirrors legacy's Win32 COM_NameForFunction, which likewise
// returns a name pointer that lives as long as the module (there it is a
// copy owned by the dll_user_t; here it is the image's own memory — see
// win32/sys.cpp for the deviation this enables).
// Win32: walks the mapped PE export directory of |lib| directly (no file
// re-read, unlike legacy's file-based LibraryLoadSymbols). Only the
// AddressOfNames-sized arrays are walked, so an export present by ordinal
// only (no name) can never match — same implicit skip as legacy's
// LibraryLoadSymbols (lib_win.c:202-278, sized off NumberOfNames). Forwarder
// exports (AddressOfFunctions RVA lands inside the export directory's own
// address range, e.g. "NTDLL.RtlAllocateHeap") are explicitly detected and
// skipped — legacy's COM_NameForFunction (lib_win.c:581-601) does NOT check
// for this and would treat a forwarder's RVA as a code offset; this is a
// deliberate bug-fix deviation, not a parity break (matching a forwarder
// would produce a nonsensical result, never observed today since none of
// the engine's own DLLs export forwarders).
// POSIX: dladdr() — a glibc/BSD libc extension, exactly like legacy's
// lib_posix.c:153-166, which likewise ignores its hInstance parameter
// entirely on POSIX (dladdr resolves ANY process address to its containing
// shared object, whichever one that is). |lib| is required to be non-null
// but is NOT cross-checked against the resolved object: dlopen()'s returned
// handle is an opaque, implementation-defined token (e.g. on glibc a
// link_map pointer, not the module's load address), so there is no
// portable way to verify it against dladdr()'s dli_fbase. The caller is
// responsible for passing an |addr| that belongs to |lib| — same
// requirement legacy has, just not enforceable here either.
[[nodiscard]] std::optional<std::string_view> name_for_symbol( LibHandle lib, const void *addr ) noexcept;

// Callback invoked once per named export by enumerate_exports().
// |name| and |addr| have the same @lifetime as name_for_symbol()'s result.
using ExportVisitor = void (*)( std::string_view name, const void *addr, void *userdata ) noexcept;

// Invoke |visit( name, addr, userdata )| once for every named, non-forwarder,
// non-ordinal-only export of |lib|, in export-table order.
// Returns false (visit is never called) for a null |lib| or a null |visit|.
// Win32: walks the mapped PE export directory (same walk and same
// forwarder/ordinal-only handling as name_for_symbol()); returns true once
// the directory is located, even if |lib| happens to export nothing named.
// POSIX: **unsupported** — dladdr() has no enumeration primitive, and
// lib_posix.c has no equivalent capability at all (it never builds an
// ordinals table; COM_FunctionFromName resolves purely by name via dlsym).
// Always returns false on POSIX; mirrors legacy's capability level exactly
// rather than inventing one (e.g. by parsing ELF section headers by hand).
[[nodiscard]] bool enumerate_exports( LibHandle lib, ExportVisitor visit, void *userdata ) noexcept;

// ---------------------------------------------------------------------------
// System paths
// ---------------------------------------------------------------------------

// Directory that contains the engine executable.
// Always ends with a forward slash; never contains backslashes.
// Example: "/home/user/xash3d-fwgs/build/"  or  "C:/xash3d/run/"
// Legacy: no direct equivalent; partially derived from argv[0] via COM_FileBase
[[nodiscard]] std::string get_executable_dir();

// Current working directory.
// Always ends with a forward slash; never contains backslashes.
[[nodiscard]] std::string get_working_directory();

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Returns true when a debugger is attached to this process.
// Returns false on platforms where detection is not implemented.
// Legacy: Platform_DebuggerPresent
[[nodiscard]] bool is_debugger_present() noexcept;

// ---------------------------------------------------------------------------
// User interaction (best-effort; degrades gracefully on headless builds)
// ---------------------------------------------------------------------------

// Display a modal error dialog.  Falls back to stderr on headless / embedded.
// Legacy: Platform_MessageBox
void message_box( std::string_view title, std::string_view message ) noexcept;

// Ask the OS to open |path| with its default handler (browser, file manager…).
// Fire-and-forget; errors are silently swallowed.
// Legacy: Platform_ShellExecute
void shell_execute( std::string_view path, std::string_view params ) noexcept;

} // namespace xash::platform
