// xash3dpp — platform (POSIX) — crash handler and stack trace
// Legacy reference: engine/platform/posix/crash_posix.c
//                   engine/platform/posix/crash_libbacktrace.c
//
// print_trace() uses backtrace_symbols_fd() which writes directly to a
// file descriptor — no malloc, async-signal-safe.

#if defined(_WIN32)
#  error "This file is POSIX-only"
#endif

#include <xash3dpp/platform/crash.hpp>

#include <execinfo.h>    // backtrace, backtrace_symbols_fd
#include <signal.h>      // sigaction, siginfo_t, SIGSEGV…
#include <unistd.h>      // STDERR_FILENO, write

#include <cstring>       // strlen

namespace xash::platform::crash {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// async-signal-safe write of a literal string to stderr.
void stderr_puts( const char *s ) noexcept
{
    const std::size_t len = std::strlen( s );
    std::size_t written = 0;
    while( written < len )
    {
        ssize_t n = ::write( STDERR_FILENO, s + written, len - written );
        if( n < 0 && errno != EINTR ) break;
        if( n > 0 ) written += static_cast<std::size_t>( n );
    }
}

void posix_fault_handler( int signum ) noexcept
{
    const char *name = ( signum == SIGSEGV ) ? "SIGSEGV" :
                       ( signum == SIGFPE  ) ? "SIGFPE"  :
                       ( signum == SIGILL  ) ? "SIGILL"  :
                       ( signum == SIGBUS  ) ? "SIGBUS"  :
                       ( signum == SIGABRT ) ? "SIGABRT" : "SIGNAL";
    stderr_puts( "*** " );
    stderr_puts( name );
    stderr_puts( " — stack trace:\n" );
    print_trace();

    // Restore the default handler and re-raise so the OS records the correct
    // exit status and produces a core dump if configured.
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
    sa.sa_handler = posix_fault_handler;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESETHAND;  // restore default on re-entry

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
    static void *frames[64];
    int n = ::backtrace( frames, 64 );
    ::backtrace_symbols_fd( frames, n, STDERR_FILENO );
}

} // namespace xash::platform::crash
