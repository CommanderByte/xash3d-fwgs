#pragma once
// xash3dpp — platform logging contract
// Legacy reference: engine/common/con_utils.c (Con_Printf, Con_DPrintf),
//                   engine/common/host.c (Host_Error, Msg)
//
// Design notes (QI, design-paradigms-round2.md):
//   • Free-function API only.  No Init / Shutdown — callable before EngineContext.
//   • Two functions: log() for pre-formatted messages, logf() for printf-style.
//     Both are noexcept and do zero heap allocation in the hot path.
//   • Output is routed: stderr (or platform console) always receives the text.
//     An optional callback hook (log_set_callback) allows the diagnostics
//     subsystem to intercept and re-route messages once it is initialised.
//   • Thread-safe: the default stderr sink is safe to call from any thread.
//     Callback registration must happen from the main thread before worker
//     threads are spawned.
//   • LogLevel::Verbose is conditionally compiled: nothing is emitted unless
//     XASH_VERBOSE is defined at build time (avoids I/O pressure in release).
//   • The Fatal level is informational only — this function does not abort.
//     Callers that need to abort after logging should call platform::crash::abort
//     (which is defined in platform/crash.hpp) separately, or use XASH_FATAL.
//
// Typical usage:
//   platform::log(LogLevel::Warning, "filesystem", "path too long");
//   platform::logf(LogLevel::Error, "memory", "pool overflow at %zu", bytes);

#include <string_view>
#include <cstdarg>       // va_list, va_start, va_end
#include <cstddef>       // std::size_t

namespace xash::platform {

// ---------------------------------------------------------------------------
// Log level
// ---------------------------------------------------------------------------

enum class LogLevel
{
    Verbose,  // detailed trace; compiled out unless XASH_VERBOSE is defined
    Info,     // normal informational message
    Warning,  // unexpected but recoverable condition
    Error,    // operation failed; caller will propagate an error return
    Fatal,    // unrecoverable invariant; caller is expected to abort after this
};

// ---------------------------------------------------------------------------
// Callback type
// ---------------------------------------------------------------------------

// Signature for a log callback registered via log_set_callback().
// Called synchronously in the context of log() / logf().
// The callback must not call log() / logf() itself (no re-entrancy).
// |level|  — severity of the message.
// |tag|    — NUL-terminated subsystem tag (e.g. "filesystem").
// |text|   — NUL-terminated formatted message, without a trailing newline.
using LogCallback = void ( * )( LogLevel level,
                                std::string_view tag,
                                std::string_view text ) noexcept;

// ---------------------------------------------------------------------------
// Core API
// ---------------------------------------------------------------------------

// Emit a pre-formatted log message.
// |tag|  — short subsystem name used in the "[tag][LEVEL]:" prefix.
// |text| — message body.  A trailing newline is appended by the implementation
//           if the text does not already end with one.
void log( LogLevel level, std::string_view tag, std::string_view text ) noexcept;

// Emit a printf-style log message.  No heap allocation — uses a fixed-size
// stack buffer (see log.cpp for the exact size).  Messages that exceed the
// buffer are silently truncated with a "..." suffix.
[[gnu::format( printf, 3, 4 )]]
void logf( LogLevel level, std::string_view tag, const char *fmt, ... ) noexcept;

// va_list variant for wrappers that already have a vararg list.
void log_va( LogLevel level, std::string_view tag,
             const char *fmt, va_list args ) noexcept;

// ---------------------------------------------------------------------------
// Callback registration
// ---------------------------------------------------------------------------

// Install a callback that receives every log() / logf() call.
// Pass nullptr to remove the current callback and revert to the default sink.
// Must be called from the main thread before any worker threads are spawned.
// Only one callback is supported at a time; calling this a second time
// replaces the previous callback.
//
// The callback is invoked in addition to — not instead of — the default sink
// (stderr / platform console).  If you want callback-only output, have the
// callback suppress the default sink by replacing platform::console::write.
void log_set_callback( LogCallback callback ) noexcept;

// ---------------------------------------------------------------------------
// Convenience level-specific helpers
// ---------------------------------------------------------------------------
// These thin wrappers are defined inline to avoid the per-call overhead of
// a level parameter; the compiler can eliminate the body when a level is
// compiled out.

#ifdef XASH_VERBOSE
inline void log_verbose( std::string_view tag, std::string_view text ) noexcept
{
    log( LogLevel::Verbose, tag, text );
}
#else
inline void log_verbose( std::string_view, std::string_view ) noexcept {}
#endif

inline void log_info( std::string_view tag, std::string_view text ) noexcept
{
    log( LogLevel::Info, tag, text );
}

inline void log_warning( std::string_view tag, std::string_view text ) noexcept
{
    log( LogLevel::Warning, tag, text );
}

inline void log_error( std::string_view tag, std::string_view text ) noexcept
{
    log( LogLevel::Error, tag, text );
}

inline void log_fatal( std::string_view tag, std::string_view text ) noexcept
{
    log( LogLevel::Fatal, tag, text );
}

} // namespace xash::platform
