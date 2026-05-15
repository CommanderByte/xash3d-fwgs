// xash3dpp — platform subsystem tests
// Covers: get_time (monotonic), sleep (no-op), open_library (error path),
//         get_symbol (null handle), close_library (null handle no-op),
//         get_executable_dir (non-empty, trailing slash, forward slashes),
//         get_working_directory (non-empty, trailing slash),
//         is_debugger_present (no crash)

#include <xash3dpp/platform/platform.hpp>

#include <cstring>   // strchr

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

static void test_get_time_monotonic()
{
    double t0 = xash::platform::get_time();
    double t1 = xash::platform::get_time();
    // Second call must not be earlier than first.
    CHECK( t1 >= t0 );
    // Legacy: Platform_DoubleTime always returned a non-negative offset.
    CHECK( t0 >= 0.0 );
}

// ---------------------------------------------------------------------------
// Sleep
// ---------------------------------------------------------------------------

static void test_sleep_zero()
{
    // sleep(0) must not hang or crash.
    xash::platform::sleep( 0 );
    ++g_pass;
}

// ---------------------------------------------------------------------------
// Library loading — error paths
// ---------------------------------------------------------------------------

static void test_open_library_bad_path()
{
    // A path that cannot point to a library on any supported platform.
    auto h = xash::platform::open_library( "\x01\x02\x03_xash3dpp_no_such_lib.dll" );
    CHECK( !h );
}

static void test_open_library_empty_path()
{
    // Empty path must return a null handle without crashing.
    // (POSIX dlopen(NULL,...) opens the main program — we must not pass NULL.)
    auto h = xash::platform::open_library( "" );
    CHECK( !h );
}

static void test_get_symbol_null_handle()
{
    // get_symbol on a null handle must return nullptr.
    xash::platform::LibHandle h{};
    CHECK( xash::platform::get_symbol( h, "anything" ) == nullptr );
}

static void test_close_library_null()
{
    // Closing a null handle must be a no-op and leave the handle null.
    xash::platform::LibHandle h{};
    xash::platform::close_library( h );
    CHECK( !h );
}

// ---------------------------------------------------------------------------
// System paths
// ---------------------------------------------------------------------------

static void test_get_executable_dir()
{
    auto d = xash::platform::get_executable_dir();
    // Must return a non-empty string.
    CHECK( !d.empty() );
    // Must end with a forward slash.
    CHECK( !d.empty() && d.back() == '/' );
    // Must not contain any backslash (forward slashes only).
    CHECK( d.find( '\\' ) == std::string::npos );
}

static void test_get_working_directory()
{
    auto d = xash::platform::get_working_directory();
    CHECK( !d.empty() );
    CHECK( !d.empty() && d.back() == '/' );
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

static void test_is_debugger_present_no_crash()
{
    // We cannot assert a specific value in a portable test runner, but the
    // function must not crash or cause undefined behaviour.
    (void)xash::platform::is_debugger_present();
    ++g_pass;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_get_time_monotonic();
    test_sleep_zero();
    test_open_library_bad_path();
    test_open_library_empty_path();
    test_get_symbol_null_handle();
    test_close_library_null();
    test_get_executable_dir();
    test_get_working_directory();
    test_is_debugger_present_no_crash();

    std::printf( "platform: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
