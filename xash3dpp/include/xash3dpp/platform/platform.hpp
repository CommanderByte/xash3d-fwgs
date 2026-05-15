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

#include <string>
#include <string_view>

namespace xash::platform {

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

// Monotonic wall-clock time in seconds.  First call initialises the epoch.
// Subsequent calls return elapsed seconds since that first call.
// Legacy: Platform_DoubleTime → Sys_DoubleTime
double get_time() noexcept;

// Yield the current thread for at least |ms| milliseconds.
// Passing 0 is a legal hint to the scheduler; it may return immediately.
// Legacy: Platform_Sleep
void sleep( unsigned ms ) noexcept;

// ---------------------------------------------------------------------------
// Dynamic library loading
// ---------------------------------------------------------------------------

// Opaque handle to a loaded shared library.
// Default-constructed value represents "no library loaded".
struct LibHandle
{
    void *native = nullptr;
    explicit operator bool() const noexcept { return native != nullptr; }
};

// Load the shared library at |path| (UTF-8).  Returns {} on failure.
// Win32: converts to UTF-16 and calls LoadLibraryW.
// POSIX: calls dlopen with RTLD_NOW | RTLD_LOCAL.
// Legacy: Sys_LoadLibrary (the raw handle half; the export-table walk
//         belongs to xash3dpp_utilities dynlib helpers).
LibHandle open_library( std::string_view path ) noexcept;

// resolve a symbol by name from a loaded library.  Returns nullptr on failure.
// Behaviour for a null LibHandle is defined: returns nullptr immediately.
// Legacy: GetProcAddress / dlsym
void *get_symbol( LibHandle lib, const char *name ) noexcept;

// Unload a shared library and zero the handle.
// Calling with a null handle is a no-op.
// Legacy: Sys_FreeLibrary
void close_library( LibHandle &lib ) noexcept;

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
