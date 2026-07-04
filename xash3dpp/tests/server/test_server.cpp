// xash3dpp — server subsystem tests
// Covers: init/shutdown lifecycle (scaffold). Functional coverage grows
// with the Chunk 6 slices (see the session ladder in implementation-plan.md).

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/server/server.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static void test_init_shutdown()
{
    xash::server::Server s;

    CHECK( !s.active() );
    CHECK( !s.initialized() );

    CHECK( s.init( xash::server::ServerInitParams{} ) );
    s.shutdown();

    // Re-init must work (idempotent lifecycle).
    CHECK( s.init( xash::server::ServerInitParams{} ) );
    s.shutdown();

    CHECK( !s.active() );
    CHECK( !s.initialized() );
}

static void test_stats_start_at_zero()
{
    xash::server::Server s;
    CHECK( s.init( xash::server::ServerInitParams{} ) );
    CHECK_EQ( s.stats().frames_run.load( std::memory_order_relaxed ),
              std::uint64_t{ 0 } );
    s.shutdown();
}

static void test_move_transfers_ownership()
{
    xash::server::Server a;
    CHECK( a.init( xash::server::ServerInitParams{} ) );

    xash::server::Server b{ static_cast<xash::server::Server &&>( a ) };
    CHECK( !b.active() );
    b.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_shutdown );
    RUN_TEST( test_stats_start_at_zero );
    RUN_TEST( test_move_transfers_ownership );

    std::printf( "server: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
