// xash3dpp — content (model cache) tests
// Covers: init/shutdown lifecycle (idempotent). Model lookup, format dispatch,
// and studio header parse tests land with the O-1 implementation.

#include <xash3dpp/content/content.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static void test_init_shutdown()
{
    // Not activated — ModelCache only borrows the reference (P-3 context).
    xash::filesystem::Filesystem fs;
    xash::content::ModelCache cache;
    CHECK( cache.init( { fs } ) );
    CHECK_EQ( cache.stats().models_loaded, 0u );
    cache.shutdown();
    // Re-init must work (idempotent lifecycle).
    CHECK( cache.init( { fs } ) );
    cache.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // The lifecycle entry points assert ThreadRole::Main; register it first.
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_shutdown );

    std::printf( "test_content: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
