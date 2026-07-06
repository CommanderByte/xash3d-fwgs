// xash3dpp — Host subsystem tests
// Covers: init/shutdown lifecycle, dedicated flag, RequestShutdown,
//         signal_frame_abort + recovery, bugcomp pass-through.

#include <xash3dpp/host/host.hpp>
#include <xash3dpp/core/error.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static xash::HostInitParams make_dedicated_params()
{
    xash::HostInitParams p;
    p.rootdir   = ".";
    p.basedir   = "valve";
    p.gamedir   = "valve";
    p.dedicated = true;
    p.developer = 0;
    // dep pointers left null — standalone / test mode
    return p;
}

static void test_init_shutdown()
{
    xash::Host host;
    CHECK( host.status() == xash::HostStatus::Init );

    const auto params = make_dedicated_params();
    REQUIRE( host.init(params) );
    CHECK( host.status() == xash::HostStatus::Running );
    CHECK( host.dedicated() );

    host.RequestShutdown( "test" );
    CHECK( host.status() == xash::HostStatus::Shutdown );
}

// ---------------------------------------------------------------------------
// Bugcomp pass-through (Resolved-decision OQ-7)
// ---------------------------------------------------------------------------

static void test_bugcomp_default_zero()
{
    xash::HostInitParams params = make_dedicated_params();
    CHECK_EQ( params.bugcomp, 0u );
}

// ---------------------------------------------------------------------------
// signal_frame_abort (Quirk Q-3, Resolved-decision OQ-1)
// ---------------------------------------------------------------------------

static void test_signal_frame_abort()
{
    xash::Host host;
    REQUIRE( host.init( make_dedicated_params() ) );

    CHECK( !host.frame_abort_pending() );
    host.signal_frame_abort( xash::core::ErrorCode::HostFatal, "test abort" );
    CHECK( host.frame_abort_pending() );
    CHECK( host.frame_abort_code() == xash::core::ErrorCode::HostFatal );

    // Running a frame should clear the abort flag (recovery at frame top).
    host.RunFrame();
    CHECK( !host.frame_abort_pending() );
    CHECK( host.frame_abort_code() == xash::core::ErrorCode::Ok );

    host.RequestShutdown( "test" );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // Host public entries assert ThreadRole::Main (QN thread-assert policy);
    // present as the engine main thread, as the launcher / production does.
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_shutdown );
    RUN_TEST( test_bugcomp_default_zero );
    RUN_TEST( test_signal_frame_abort );

    std::printf( "host: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
