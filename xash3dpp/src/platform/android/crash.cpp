// xash3dpp — platform (Android) — crash handler and stack trace
// Legacy reference: engine/platform/posix/crash_posix.c (signal handler),
//                   Android NDK _Unwind_Backtrace (bionic has no execinfo.h).
//
// Stack trace uses _Unwind_Backtrace (from <unwind.h>) which is available in
// all NDK versions.  Frames are written to logcat at ERROR priority.

#if !defined(XASH_ANDROID)
#  error "This file is Android-only (XASH_ANDROID must be defined)"
#endif

#include <xash3dpp/platform/crash.hpp>
#include <xash3dpp/limits.hpp>

#include <android/log.h>
#include <signal.h>
#include <unwind.h>

#include <cstdio>    // snprintf
#include <cstdint>   // uintptr_t

namespace xash::platform::crash {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

struct UnwindState {
    void  **current;
    void  **end;
};

_Unwind_Reason_Code unwind_callback( struct _Unwind_Context *ctx, void *arg ) noexcept
{
    UnwindState *state = static_cast<UnwindState *>( arg );
    if( state->current == state->end ) return _URC_END_OF_STACK;
    *state->current++ = reinterpret_cast<void *>( _Unwind_GetIP( ctx ) );
    return _URC_NO_REASON;
}

void android_fault_handler( int signum ) noexcept
{
    const char *name = ( signum == SIGSEGV ) ? "SIGSEGV" :
                       ( signum == SIGFPE  ) ? "SIGFPE"  :
                       ( signum == SIGILL  ) ? "SIGILL"  :
                       ( signum == SIGBUS  ) ? "SIGBUS"  :
                       ( signum == SIGABRT ) ? "SIGABRT" : "SIGNAL";
    __android_log_print( ANDROID_LOG_FATAL, "xash3dpp_crash",
                         "*** %s — stack trace follows:", name );
    print_trace();

    // Restore default and re-raise.
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset( &sa.sa_mask );
    sigaction( signum, &sa, nullptr );
    raise( signum );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// install_handler
// ---------------------------------------------------------------------------

void install_handler() noexcept
{
    static bool installed = false;
    if( installed ) return;
    installed = true;

    struct sigaction sa{};
    sa.sa_handler = android_fault_handler;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESETHAND;

    sigaction( SIGSEGV, &sa, nullptr );
    sigaction( SIGFPE,  &sa, nullptr );
    sigaction( SIGILL,  &sa, nullptr );
    sigaction( SIGBUS,  &sa, nullptr );
    sigaction( SIGABRT, &sa, nullptr );
}

// ---------------------------------------------------------------------------
// print_trace
// ---------------------------------------------------------------------------

void print_trace() noexcept
{
    static void *buffer[::xash::limits::platform_crash_frames_max];
    UnwindState state = { buffer, buffer + ::xash::limits::platform_crash_frames_max };
    _Unwind_Backtrace( unwind_callback, &state );

    const int count = static_cast<int>( state.current - buffer );
    char line[48];
    for( int i = 0; i < count; ++i )
    {
        std::snprintf( line, sizeof( line ), "#%02d  %p",
                       i, buffer[i] );
        __android_log_write( ANDROID_LOG_FATAL, "xash3dpp_crash", line );
    }
}

} // namespace xash::platform::crash
