// xash3dpp — platform (Android) — console I/O (logcat bridge)
// Legacy reference: engine/platform/android/ — Android has no system console.
//
// write() forwards to logcat so developer output is visible in `adb logcat`.
// read_line() is a no-op: there is no terminal input channel.

#if !defined(XASH_ANDROID)
#  error "This file is Android-only (XASH_ANDROID must be defined)"
#endif

#include <xash3dpp/platform/console.hpp>

#include <android/log.h>

#include <cstring>    // memcpy

namespace xash::platform::console {

// ---------------------------------------------------------------------------
// write — forward to logcat at DEBUG priority.
// ---------------------------------------------------------------------------

void write( std::string_view text ) noexcept
{
    if( text.empty() ) return;

    // __android_log_write requires a null-terminated string.
    // Copy into a static buffer, truncating if necessary.
    static char buf[1024];
    const std::size_t copy_len = text.size() < sizeof( buf ) - 1
                                     ? text.size()
                                     : sizeof( buf ) - 1;
    // Strip a trailing newline so logcat doesn't add a blank line.
    std::size_t len = copy_len;
    while( len > 0 && ( text[len - 1] == '\n' || text[len - 1] == '\r' ) )
        --len;

    std::memcpy( buf, text.data(), len );
    buf[len] = '\0';

    __android_log_write( ANDROID_LOG_DEBUG, "xash3dpp", buf );
}

// ---------------------------------------------------------------------------
// read_line — no terminal on Android.
// ---------------------------------------------------------------------------

std::string_view read_line() noexcept
{
    return {};
}

} // namespace xash::platform::console
