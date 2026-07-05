// xash3dpp — core logging implementation
// Design: see include/xash3dpp/core/log.hpp for the full contract.
//
// Implementation notes:
//   • A single std::atomic<LogCallback> slot stores the optional callback.
//     Atomic load/store with relaxed ordering is sufficient: the callback
//     pointer itself is always written before worker threads start, and we
//     do not need acquire/release visibility into what the callback does.
//   • Formatting uses a fixed-size stack buffer to avoid heap allocation.
//     Messages that exceed the buffer are truncated and end with " [...]".
//   • The default output sink is platform::console::write, which handles
//     per-platform differences (Win32 write_file vs POSIX write vs logcat).
//   • The tag is printed as "[tag][LEVEL]: " — matching the legacy Con_Printf
//     prefix that the GoldSrc game DLLs and users recognise.

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/platform/console.hpp>
#include <xash3dpp/limits.hpp>

#include <atomic>
#include <cstdio>    // std::vsnprintf
#include <cstring>   // std::strlen, std::memcpy
#include <cstddef>   // std::size_t

namespace xash::core {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

namespace {

// Single optional callback.  Written once from the main thread; thereafter
// only read.  Relaxed ordering is safe: see file header.
std::atomic<LogCallback> g_log_callback{ nullptr };  // compliance-allow(mutable-global, di-global-ref): diagnostics-layer atomic callback — the log seam is deliberately global (no context exists at log time); G-1 extension hook (log_set_callback)

// Level abbreviations for the "[tag][LEVEL]:" prefix.
const char *level_tag( LogLevel level ) noexcept
{
    switch( level )
    {
    case LogLevel::Verbose: return "VERB";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error:   return "ERR ";
    case LogLevel::Fatal:   return "FATAL";
    }
    return "????";
}

// Build the header "[tag][LEVEL]: " into |dst|, returning the number of
// characters written (not including the NUL terminator).
// |dst_size| must be > 0.
std::size_t format_prefix( char *dst, std::size_t dst_size,
                            std::string_view tag, LogLevel level ) noexcept
{
    // std::vsnprintf always NUL-terminates when dst_size > 0.
    int n = std::snprintf( dst, dst_size, "[%.*s][%s]: ",
                           static_cast<int>( tag.size() ), tag.data(),
                           level_tag( level ) );
    if( n < 0 ) return 0;
    return static_cast<std::size_t>( n ) < dst_size
               ? static_cast<std::size_t>( n )
               : dst_size - 1;
}

// Truncation marker appended when the body is cut off.
inline constexpr std::string_view k_truncation_marker = " [...]";

// Emit the final, complete line to console and optional callback.
void emit( LogLevel level, std::string_view tag,
           char *buf, std::size_t body_start, std::size_t body_end ) noexcept
{
    // Ensure the message ends with exactly one newline.
    std::size_t end = body_end;
    constexpr std::size_t k_buf = xash::limits::platform_log_buffer_size;
    if( end == 0 || buf[end - 1] != '\n' )
    {
        if( end < k_buf - 1 )
            buf[end++] = '\n';
        else
            buf[k_buf - 2] = '\n', end = k_buf - 1;
    }
    buf[end] = '\0';

    std::string_view full_line{ buf, end };

    // Default sink: platform console.
    ::xash::platform::console::write( full_line );

    // Optional callback (body only, without the prefix and newline).
    LogCallback cb = g_log_callback.load( std::memory_order_relaxed );
    if( cb )
    {
        // Pass body text without the trailing newline.
        std::string_view body_text{ buf + body_start,
                                    end > body_start + 1
                                        ? end - body_start - 1
                                        : 0 };
        cb( level, tag, body_text );
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API — log()
// ---------------------------------------------------------------------------

void log( LogLevel level, std::string_view tag, std::string_view text ) noexcept
{
#ifndef XASH_VERBOSE
    if( level == LogLevel::Verbose ) return;
#endif

    constexpr std::size_t k_buf = xash::limits::platform_log_buffer_size;
    char buf[k_buf];

    // Write "[tag][LEVEL]: " prefix.
    std::size_t prefix_len = format_prefix( buf, k_buf, tag, level );

    // Copy body, truncating if necessary.
    std::size_t body_start = prefix_len;
    std::size_t remaining  = k_buf - prefix_len - 2; // reserve space for '\n' + '\0'
    std::size_t body_len   = text.size();
    bool truncated = false;

    if( body_len > remaining )
    {
        // Reserve room for the truncation marker.
        if( remaining > k_truncation_marker.size() )
            remaining -= k_truncation_marker.size();
        body_len  = remaining;
        truncated = true;
    }

    std::memcpy( buf + prefix_len, text.data(), body_len );
    std::size_t pos = prefix_len + body_len;

    if( truncated )
    {
        std::memcpy( buf + pos, k_truncation_marker.data(),
                     k_truncation_marker.size() );
        pos += k_truncation_marker.size();
    }

    emit( level, tag, buf, body_start, pos );
}

// ---------------------------------------------------------------------------
// Public API — log_va()
// ---------------------------------------------------------------------------

void log_va( LogLevel level, std::string_view tag,
             const char *fmt, va_list args ) noexcept
{
#ifndef XASH_VERBOSE
    if( level == LogLevel::Verbose ) return;
#endif

    constexpr std::size_t k_buf = xash::limits::platform_log_buffer_size;
    char buf[k_buf];

    std::size_t prefix_len = format_prefix( buf, k_buf, tag, level );
    std::size_t body_start = prefix_len;

    // Format the body into the remainder of the buffer.
    std::size_t remaining = k_buf - prefix_len; // vsnprintf handles the NUL
    int n = std::vsnprintf( buf + prefix_len,
                            remaining,
                            fmt, args );

    std::size_t body_end;
    if( n < 0 )
    {
        // Encoding error — leave empty body.
        body_end = prefix_len;
    }
    else if( static_cast<std::size_t>( n ) >= remaining )
    {
        // vsnprintf truncated.  Append truncation marker (best-effort; may
        // itself be truncated if the buffer is almost full).
        body_end = k_buf - 1; // vsnprintf wrote k_buf-1 chars + NUL
        std::size_t marker_start = ( k_buf - 1 > k_truncation_marker.size() )
                                   ? k_buf - 1 - k_truncation_marker.size()
                                   : prefix_len;
        std::memcpy( buf + marker_start,
                     k_truncation_marker.data(),
                     k_truncation_marker.size() );
    }
    else
    {
        body_end = prefix_len + static_cast<std::size_t>( n );
    }

    emit( level, tag, buf, body_start, body_end );
}

// ---------------------------------------------------------------------------
// Public API — logf()
// ---------------------------------------------------------------------------

void logf( LogLevel level, std::string_view tag, const char *fmt, ... ) noexcept
{
    va_list args;
    va_start( args, fmt );
    log_va( level, tag, fmt, args );
    va_end( args );
}

// ---------------------------------------------------------------------------
// Public API — log_set_callback()
// ---------------------------------------------------------------------------

void log_set_callback( LogCallback callback ) noexcept
{
    g_log_callback.store( callback, std::memory_order_relaxed );
}

} // namespace xash::core
