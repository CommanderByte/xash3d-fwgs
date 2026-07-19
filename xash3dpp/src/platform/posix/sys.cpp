// xash3dpp — platform (POSIX: Linux, macOS, FreeBSD, Android) — process-level services
// Legacy reference: engine/platform/posix/sys_posix.c,
//                  engine/common/system.h
//
// Existing subsystems used:
//   xash3dpp_utilities — path::extract_dir, path::fix_slashes

#if defined(_WIN32)
#  error "This file is POSIX-only"
#endif

#include <xash3dpp/platform/platform.hpp>
#include <xash3dpp/utilities/path.hpp>

#include <time.h>       // clock_gettime, nanosleep, CLOCK_MONOTONIC
#include <dlfcn.h>      // dlopen, dlsym, dlclose, RTLD_NOW, RTLD_LOCAL
#include <unistd.h>     // getcwd, readlink, fork, execvp, _exit, read, close
#include <sys/types.h>  // pid_t, ssize_t
#include <fcntl.h>      // open, O_RDONLY (for debugger detection)
#include <cstdint>      // std::uint32_t
#include <cstring>      // std::memcpy, std::strstr
#include <cstdio>       // std::fprintf
#include <cerrno>       // errno, EINTR
#include <optional>     // std::optional (name_for_symbol)

#include <xash3dpp/private/core/assert_main.hpp>

#if defined(__APPLE__)
#  include <mach-o/dyld.h>  // _NSGetExecutablePath
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  include <sys/sysctl.h>
#endif

#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

namespace xash::platform {

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

double get_time() noexcept
{
    auto read_clock = []() noexcept -> double {
        struct timespec ts;
        clock_gettime( CLOCK_MONOTONIC, &ts );
        return static_cast<double>( ts.tv_sec )
             + static_cast<double>( ts.tv_nsec ) * 1e-9;
    };
    // Initialised exactly once on first call (C++11 magic-static, thread-safe).
    static const double s_epoch = read_clock();
    // Capture main-thread ID on first call (inside magic-static — thread-safe).
    static const bool s_main_captured = []() noexcept {
        ::xash::core::detail::capture_main_thread();
        return true;
    }();
    (void)s_main_captured;
    return read_clock() - s_epoch;
}

void sleep( std::uint32_t ms ) noexcept
{
    struct timespec ts{
        static_cast<time_t>( ms / 1000u ),
        static_cast<long>( ( ms % 1000u ) * 1'000'000L )
    };
    // Restart on signal interruption.
    while( nanosleep( &ts, &ts ) == -1 && errno == EINTR ) {}
}

// ---------------------------------------------------------------------------
// Dynamic library loading
// ---------------------------------------------------------------------------

LibHandle open_library( std::string_view path ) noexcept
{
    if( path.empty() )
        return {};

    // dlopen requires a null-terminated string.
    char buf[PATH_MAX];
    std::size_t n = path.size() < sizeof( buf ) - 1 ? path.size() : sizeof( buf ) - 1;
    std::memcpy( buf, path.data(), n );
    buf[n] = '\0';

    void *h = dlopen( buf, RTLD_NOW | RTLD_LOCAL );
    return { h };
}

void *get_symbol( LibHandle lib, const char *name ) noexcept
{
    if( !lib )
        return nullptr;
    return dlsym( lib.native, name );
}

void close_library( LibHandle &lib ) noexcept
{
    if( !lib )
        return;
    dlclose( lib.native );
    lib = {};
}

// ---------------------------------------------------------------------------
// Dynamic library reverse lookup (SAV-OQ-3) — see platform.hpp for the full
// contract.
// ---------------------------------------------------------------------------

std::optional<std::string_view> name_for_symbol( LibHandle lib, const void *addr ) noexcept
{
    if( !lib || !addr )
        return std::nullopt;

    // NOTE: dladdr() is a glibc/BSD libc extension, not POSIX — same caveat
    // as legacy's lib_posix.c:155 comment. It resolves ANY process address
    // to its containing shared object + nearest symbol; |lib| is not
    // consulted by the lookup itself, exactly like legacy's
    // COM_NameForFunction (lib_posix.c:153-166), which ignores its
    // hInstance parameter entirely on POSIX.
    //
    // deliberately NOT cross-checked against |lib|: dlopen()'s returned
    // handle is an opaque, implementation-defined token (on glibc it is a
    // pointer to internal link_map bookkeeping, NOT the module's load
    // address) — there is no portable, standard-library way to verify it
    // equals dladdr()'s dli_fbase. |lib| is still required to be non-null
    // (a real caller error otherwise) but the caller is responsible for
    // passing an |addr| that actually belongs to |lib|, same as legacy.
    Dl_info info{};
    if( dladdr( addr, &info ) == 0 || info.dli_sname == nullptr )
        return std::nullopt;

    return std::string_view{ info.dli_sname };
}

bool enumerate_exports( LibHandle /*lib*/, ExportVisitor /*visit*/, void * /*userdata*/ ) noexcept
{
    // Unsupported on POSIX: dladdr() maps address -> symbol but has no
    // enumeration primitive, and lib_posix.c has no equivalent at all — it
    // never builds an ordinals table (COM_FunctionFromName resolves purely
    // by name via dlsym). Mirrors legacy's capability level exactly rather
    // than inventing one (e.g. hand-parsing ELF section headers).
    return false;
}

// ---------------------------------------------------------------------------
// System paths
// ---------------------------------------------------------------------------

std::string get_executable_dir()
{
    char buf[PATH_MAX];
    buf[0] = '\0';

#if defined(__linux__) || defined(__ANDROID__)
    ssize_t n = readlink( "/proc/self/exe", buf, sizeof( buf ) - 1 );
    if( n > 0 )
        buf[n] = '\0';
    else
        buf[0] = '\0';

#elif defined(__APPLE__)
    uint32_t sz = static_cast<uint32_t>( sizeof( buf ) );
    if( _NSGetExecutablePath( buf, &sz ) != 0 )
        buf[0] = '\0';

#elif defined(__FreeBSD__)
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    std::size_t sz = sizeof( buf );
    if( sysctl( mib, 4, buf, &sz, nullptr, 0 ) != 0 )
        buf[0] = '\0';
#endif

    if( !buf[0] )
        return {};

    // extract_dir strips the filename and keeps the trailing separator.
    auto dir = ::xash::utilities::extract_dir( std::string_view{ buf } );
    // POSIX paths already use '/', but fix_slashes is a no-op in that case.
    ::xash::utilities::fix_slashes( dir );
    return dir;
}

std::string get_working_directory()
{
    char buf[PATH_MAX];
    if( !getcwd( buf, sizeof( buf ) ) )
        return {};

    std::string result{ buf };
    if( !result.empty() && result.back() != '/' )
        result += '/';
    return result;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

bool is_debugger_present() noexcept
{
#if defined(__linux__)
    // Read TracerPid from /proc/self/status.  Non-zero means a tracer is attached.
    int fd = ::open( "/proc/self/status", O_RDONLY );
    if( fd == -1 )
        return false;

    char status[4096];
    ssize_t n = ::read( fd, status, sizeof( status ) - 1 );
    ::close( fd );
    if( n <= 0 )
        return false;
    status[n] = '\0';

    const char *p = std::strstr( status, "TracerPid:" );
    if( !p )
        return false;
    p += sizeof( "TracerPid:" ) - 1;
    while( *p == ' ' || *p == '\t' )
        ++p;
    return *p != '0';
#else
    // TODO: implement for macOS (PT_ATTACHEXC) and BSD (ptrace).
    return false;
#endif
}

// ---------------------------------------------------------------------------
// User interaction
// ---------------------------------------------------------------------------

void message_box( std::string_view title, std::string_view message ) noexcept
{
    // POSIX has no portable synchronous GUI dialog.  Write to stderr so the
    // message is always visible in a terminal or log.  SDL2/X11 callers may
    // override this by calling their own dialog API before the engine reaches
    // this fallback — that is a host-layer concern.
    std::fprintf( stderr, "** %.*s **\n%.*s\n",
                  static_cast<int>( title.size() ),   title.data(),
                  static_cast<int>( message.size() ), message.data() );
}

void shell_execute( std::string_view path, std::string_view params ) noexcept
{
    char p[PATH_MAX], a[PATH_MAX];
    std::size_t plen = path.size()   < sizeof( p ) - 1 ? path.size()   : sizeof( p ) - 1;
    std::size_t alen = params.size() < sizeof( a ) - 1 ? params.size() : sizeof( a ) - 1;
    std::memcpy( p, path.data(),   plen ); p[plen] = '\0';
    std::memcpy( a, params.data(), alen ); a[alen] = '\0';

    // Fire-and-forget: fork a child that execs the OS default opener.
    // The child exits immediately so the parent never calls waitpid, but the
    // grandchild is reparented to init and cleaned up by the OS.
    // TODO: replace with double-fork to avoid zombie accumulation on long runs.
    pid_t child = fork();
    if( child < 0 )
        return;  // fork failed — silently drop
    if( child == 0 )
    {
        // Child process.
#if defined(__APPLE__)
        const char *opener = "open";
#else
        const char *opener = "xdg-open";
#endif
        const char *argv[] = { opener, p, alen ? a : nullptr, nullptr };
        execvp( opener, const_cast<char **>( argv ) ); // SAFETY: const_cast for execvp's historical char*const argv signature — POSIX guarantees the strings are not modified
        ::_exit( 1 );  // exec failed
    }
    // Parent: intentionally does not waitpid — fire-and-forget.
}

} // namespace xash::platform
