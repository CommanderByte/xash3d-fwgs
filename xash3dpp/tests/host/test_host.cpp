// xash3dpp — Host subsystem tests
// Covers: init/shutdown lifecycle, dedicated flag, RequestShutdown,
//         signal_frame_abort + recovery, bugcomp pass-through.

#include <xash3dpp/host/host.hpp>
#include <xash3dpp/core/error.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static xash::HostArgs make_dedicated_args()
{
    xash::HostArgs args;
    args.rootdir   = ".";
    args.basedir   = "valve";
    args.gamedir   = "valve";
    args.dedicated = true;
    args.developer = 0;
    return args;
}

static void test_init_shutdown()
{
    xash::Host host;
    CHECK( host.status() == xash::HostStatus::kInit );

    const auto args = make_dedicated_args();
    REQUIRE( host.init(args) );
    CHECK( host.status() == xash::HostStatus::kRunning );
    CHECK( host.dedicated() );

    host.RequestShutdown( "test" );
    CHECK( host.status() == xash::HostStatus::kShutdown );
}

// ---------------------------------------------------------------------------
// Bugcomp pass-through (Resolved-decision OQ-7)
// ---------------------------------------------------------------------------

static void test_bugcomp_default_zero()
{
    xash::HostArgs args = make_dedicated_args();
    CHECK_EQ( args.bugcomp, 0u );
}

// ---------------------------------------------------------------------------
// signal_frame_abort (Quirk Q-3, Resolved-decision OQ-1)
// ---------------------------------------------------------------------------

static void test_signal_frame_abort()
{
    xash::Host host;
    REQUIRE( host.init( make_dedicated_args() ) );

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
    RUN_TEST( test_init_shutdown );
    RUN_TEST( test_bugcomp_default_zero );
    RUN_TEST( test_signal_frame_abort );

    std::printf( "host: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
