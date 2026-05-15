// xash3dpp — platform (Win32) — process-level services
// Legacy reference: engine/platform/win32/sys_win.c,
//                  engine/common/system.h
//
// Existing subsystems used:
//   xash3dpp_utilities — path::extract_dir, path::fix_slashes

#ifndef _WIN32
#  error "This file is Win32-only"
#endif

#include <xash3dpp/platform/platform.hpp>
#include <xash3dpp/utilities/path.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <cstring>   // std::memcpy

#include "../detail/assert_main.hpp"

namespace xash::platform {

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

double get_time() noexcept
{
    // Capture QPC frequency and start counter on first call (magic-static,
    // thread-safe since C++11).
    struct ClockInit
    {
        LONGLONG freq;
        LONGLONG start;
        ClockInit() noexcept
        {
            LARGE_INTEGER f, s;
            QueryPerformanceFrequency( &f );
            QueryPerformanceCounter( &s );
            freq  = f.QuadPart;
            start = s.QuadPart;
        }
    };
    static const ClockInit s_clock{};
    // Capture main-thread ID on first call (inside magic-static — thread-safe).
    static const bool s_main_captured = []() noexcept {
        detail::capture_main_thread();
        return true;
    }();
    (void)s_main_captured;

    LARGE_INTEGER now;
    QueryPerformanceCounter( &now );
    return static_cast<double>( now.QuadPart - s_clock.start )
         / static_cast<double>( s_clock.freq );
}

void sleep( unsigned ms ) noexcept
{
    Sleep( static_cast<DWORD>( ms ) );
}

// ---------------------------------------------------------------------------
// Dynamic library loading
// ---------------------------------------------------------------------------

LibHandle open_library( std::string_view path ) noexcept
{
    // Convert UTF-8 path to UTF-16 for LoadLibraryW.
    wchar_t wbuf[1024];
    int len = MultiByteToWideChar( CP_UTF8, 0,
                                   path.data(), static_cast<int>( path.size() ),
                                   wbuf, static_cast<int>( std::size( wbuf ) ) - 1 );
    if( len <= 0 )
        return {};
    wbuf[len] = L'\0';

    HMODULE h = LoadLibraryW( wbuf );
    return { static_cast<void *>( h ) };
}

void *get_symbol( LibHandle lib, const char *name ) noexcept
{
    if( !lib )
        return nullptr;
    return reinterpret_cast<void *>(
        GetProcAddress( static_cast<HMODULE>( lib.native ), name ) );
}

void close_library( LibHandle &lib ) noexcept
{
    if( !lib )
        return;
    FreeLibrary( static_cast<HMODULE>( lib.native ) );
    lib = {};
}

// ---------------------------------------------------------------------------
// System paths
// ---------------------------------------------------------------------------

std::string get_executable_dir()
{
    wchar_t wbuf[MAX_PATH];
    DWORD len = GetModuleFileNameW( nullptr, wbuf, MAX_PATH );
    if( len == 0 )
        return {};

    char buf[MAX_PATH * 4];
    int n = WideCharToMultiByte( CP_UTF8, 0,
                                 wbuf, static_cast<int>( len ),
                                 buf, static_cast<int>( sizeof( buf ) ) - 1,
                                 nullptr, nullptr );
    if( n <= 0 )
        return {};
    buf[n] = '\0';

    // extract_dir returns the directory component with trailing separator.
    auto dir = utilities::extract_dir( std::string_view{ buf, static_cast<std::size_t>( n ) } );
    utilities::fix_slashes( dir );   // backslashes → forward slashes
    return dir;
}

std::string get_working_directory()
{
    wchar_t wbuf[MAX_PATH];
    DWORD len = GetCurrentDirectoryW( MAX_PATH, wbuf );
    if( len == 0 )
        return {};

    char buf[MAX_PATH * 4];
    int n = WideCharToMultiByte( CP_UTF8, 0,
                                 wbuf, static_cast<int>( len ),
                                 buf, static_cast<int>( sizeof( buf ) ) - 1,
                                 nullptr, nullptr );
    if( n <= 0 )
        return {};
    buf[n] = '\0';

    std::string result{ buf, static_cast<std::size_t>( n ) };
    if( result.back() != '/' && result.back() != '\\' )
        result += '/';
    utilities::fix_slashes( result );
    return result;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

bool is_debugger_present() noexcept
{
    return static_cast<bool>( IsDebuggerPresent() );
}

// ---------------------------------------------------------------------------
// User interaction
// ---------------------------------------------------------------------------

void message_box( std::string_view title, std::string_view message ) noexcept
{
    // Null-terminate stack copies — MessageBoxA requires C strings.
    char t[256], m[1024];
    std::size_t tlen = title.size()   < sizeof( t ) - 1 ? title.size()   : sizeof( t ) - 1;
    std::size_t mlen = message.size() < sizeof( m ) - 1 ? message.size() : sizeof( m ) - 1;
    std::memcpy( t, title.data(),   tlen ); t[tlen] = '\0';
    std::memcpy( m, message.data(), mlen ); m[mlen] = '\0';
    MessageBoxA( nullptr, m, t, MB_OK | MB_SETFOREGROUND | MB_ICONSTOP );
}

void shell_execute( std::string_view path, std::string_view params ) noexcept
{
    char p[1024], a[1024];
    std::size_t plen = path.size()   < sizeof( p ) - 1 ? path.size()   : sizeof( p ) - 1;
    std::size_t alen = params.size() < sizeof( a ) - 1 ? params.size() : sizeof( a ) - 1;
    std::memcpy( p, path.data(),   plen ); p[plen] = '\0';
    std::memcpy( a, params.data(), alen ); a[alen] = '\0';
    ShellExecuteA( nullptr, "open", p, alen ? a : nullptr, nullptr, SW_SHOW );
}

} // namespace xash::platform
