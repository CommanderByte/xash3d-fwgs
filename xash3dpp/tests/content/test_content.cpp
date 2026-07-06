// xash3dpp — content (model cache) tests
// Covers: init/shutdown lifecycle (idempotent). Model lookup, format dispatch,
// and studio header parse tests land with the O-1 implementation.

#include <xash3dpp/content/content.hpp>
#include <xash3dpp/content/model.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <cstdint>

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
// Model registry — typed handle lookup (chunk deliverable)
// ---------------------------------------------------------------------------

static void test_model_registry()
{
    using namespace xash::content;

    xash::filesystem::Filesystem fs;
    ModelCache cache;
    REQUIRE( cache.init( { fs } ) );

    // World is pinned to slot 0.
    const ModelHandle world = cache.register_world( "maps/c0a0.bsp" );
    CHECK( world.valid() );
    CHECK_EQ( world.index, std::uint16_t{ 0 } );
    REQUIRE( cache.resolve( world ) != nullptr );
    CHECK( cache.resolve( world )->type() == ModelType::Brush );

    // find_or_alloc never returns slot 0 and is idempotent by name.
    const ModelHandle a = cache.find_or_alloc( "models/player.mdl" );
    CHECK( a.valid() );
    CHECK( a.index != 0 );
    const ModelHandle a2 = cache.find_or_alloc( "models/player.mdl" );
    CHECK( a == a2 );                          // same slot on re-request
    CHECK_EQ( cache.stats().cache_hits, std::uint64_t{ 1 } );

    // Distinct names get distinct slots.
    const ModelHandle b = cache.find_or_alloc( "sprites/explode.spr" );
    CHECK( b.valid() );
    CHECK( b.index != a.index );

    // find() locates a registered model, or the null handle if absent.
    CHECK( cache.find( "models/player.mdl" ) == a );
    CHECK( !cache.find( "does/not/exist.mdl" ).valid() );

    // resolve exposes the model; name round-trips.
    const Model *m = cache.resolve( a );
    REQUIRE( m != nullptr );
    CHECK( m->name() == "models/player.mdl" );
    CHECK( m->needload() == NeedLoad::NeedsLoaded );

    CHECK_EQ( cache.live_count(), std::size_t{ 3 } );  // world + a + b

    // Freeing invalidates the handle (generation bump); the slot is reusable.
    cache.free_model( a );
    CHECK( cache.resolve( a ) == nullptr );            // stale handle fails
    CHECK_EQ( cache.live_count(), std::size_t{ 2 } );

    const ModelHandle c = cache.find_or_alloc( "models/gauss.mdl" );
    CHECK( c.valid() );
    CHECK_EQ( c.index, a.index );                      // reused the freed slot
    CHECK( c.generation != a.generation );             // but a fresh generation
    CHECK( cache.resolve( a ) == nullptr );            // old handle still stale
    REQUIRE( cache.resolve( c ) != nullptr );
    CHECK( cache.resolve( c )->name() == "models/gauss.mdl" );

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
    RUN_TEST( test_model_registry );

    std::printf( "test_content: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
