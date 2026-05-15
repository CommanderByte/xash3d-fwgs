// xash3dpp — platform (Win32) — crash handler and stack trace
// Legacy reference: engine/platform/win32/crash_win.c

#ifndef _WIN32
#  error "This file is Win32-only"
#endif

#include <xash3dpp/platform/crash.hpp>

#include <xash3dpp/private/core/assert_main.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>

#include <cstdio>    // snprintf
#include <cstring>   // strlen
#include <atomic>    // std::atomic

namespace xash::platform::crash {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Write a null-terminated string to stderr without heap allocation.
void stderr_puts( const char *s ) noexcept
{
    HANDLE h = GetStdHandle( STD_ERROR_HANDLE );
    if( h == INVALID_HANDLE_VALUE || h == nullptr ) return;
    DWORD written;
    WriteFile( h, s, static_cast<DWORD>( std::strlen( s ) ), &written, nullptr );
}

LONG WINAPI seh_filter( EXCEPTION_POINTERS * ) noexcept
{
    stderr_puts( "*** unhandled exception — stack trace (addresses):\n" );
    print_trace();
    return EXCEPTION_CONTINUE_SEARCH;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// install_handler
// ---------------------------------------------------------------------------

void install_handler() noexcept
{
    core::detail::assert_main_thread( "crash::install_handler" );
    static std::atomic<bool> installed{ false };
    if( installed.exchange( true ) ) return;
    SetUnhandledExceptionFilter( seh_filter );
}

// ---------------------------------------------------------------------------
// print_trace
// ---------------------------------------------------------------------------

void print_trace() noexcept
{
    static void *frames[64];
    USHORT n = CaptureStackBackTrace( 0, 64, frames, nullptr );

    HANDLE h = GetStdHandle( STD_ERROR_HANDLE );
    if( h == INVALID_HANDLE_VALUE || h == nullptr ) return;

    char line[48];
    DWORD written;
    for( USHORT i = 0; i < n; ++i )
    {
        int len = std::snprintf( line, sizeof( line ), "  [%02u] %p\n",
                                 static_cast<unsigned>( i ), frames[i] );
        if( len > 0 )
            WriteFile( h, line, static_cast<DWORD>( len ), &written, nullptr );
    }
}

} // namespace xash::platform::crash
