// xash3dpp — EdictArena behaviour pins (Chunk 6 S4, Q-20)
// Covers: init-state contract (zeroed entvars, pContainingEntity
// self-link, controller rest 0x7F), the free() scrub subset with STALE
// remainder, serialnumber bump + idempotent free, the reuse policy
// (first-seconds relax window vs 0.5s grace), growth + nullptr on
// exhaustion, private-data lifecycle with the releaser hook, and the
// byte-offset contract behind pfnPEntityOfEntOffset.

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>

#include "../../test_helpers.hpp"

namespace sv  = xash::server;
namespace abi = xash::abi;

static int g_pass = 0, g_fail = 0;

namespace {

struct ArenaFixture
{
    xash::memory::PoolHandle pool;
    sv::EdictArena           arena;

    explicit ArenaFixture( std::size_t max_edicts, std::size_t reserved )
    {
        pool = xash::memory::create_pool( "test_edict_arena" );
        REQUIRE( static_cast<bool>( pool ));
        REQUIRE( arena.init( pool, max_edicts, reserved ));
    }

    ~ArenaFixture()
    {
        arena.shutdown();
        xash::memory::destroy_pool( pool );
    }
};

} // namespace

static void test_alloc_init_state()
{
    ArenaFixture f( 16, 2 );

    CHECK_EQ( f.arena.num_entities(), 2u );

    abi::edict_t *e = f.arena.alloc_edict( 1.0 );
    REQUIRE( e != nullptr );
    CHECK_EQ( f.arena.index_of( e ), 2 );
    CHECK_EQ( f.arena.num_entities(), 3u );

    CHECK( !e->free );
    CHECK( e->v.pContainingEntity == e );
    for ( int i = 0; i < 4; ++i )
        CHECK_EQ( unsigned( e->v.controller[i] ), 0x7Fu );
    CHECK_EQ( unsigned( e->v.blending[0] ), 0u );
    CHECK_EQ( e->v.velocity[0], 0.0f );
    CHECK_EQ( e->v.health, 0.0f );
}

static void test_free_scrubs_subset_leaves_stale()
{
    ArenaFixture f( 16, 2 );

    abi::edict_t *e = f.arena.alloc_edict( 1.0 );
    REQUIRE( e != nullptr );

    e->v.health     = 55.0f;
    e->v.target     = 7;
    e->v.solid      = 3;
    e->v.flags      = 0x40;
    e->v.model      = 12;
    e->v.nextthink  = 9.0f;
    e->v.origin[0]  = 4.0f;
    e->v.angles[1]  = 5.0f;
    const int serial_before = e->serialnumber;

    f.arena.free_edict( e, 1.25 );

    CHECK( e->free != 0 );
    CHECK_EQ( e->freetime, 1.25f );
    CHECK_EQ( e->serialnumber, serial_before + 1 );

    // Scrubbed subset (sv_game.c:1017-1030)...
    CHECK_EQ( e->v.solid, 0 );
    CHECK_EQ( e->v.flags, 0 );
    CHECK_EQ( e->v.model, 0 );
    CHECK_EQ( e->v.nextthink, -1.0f );
    CHECK_EQ( e->v.origin[0], 0.0f );
    CHECK_EQ( e->v.angles[1], 0.0f );

    // ...everything else deliberately stale while free.
    CHECK_EQ( e->v.health, 55.0f );
    CHECK_EQ( e->v.target, 7 );

    // Idempotent: a second free must not bump the serial again.
    f.arena.free_edict( e, 2.0 );
    CHECK_EQ( e->serialnumber, serial_before + 1 );
    CHECK_EQ( e->freetime, 1.25f );
}

static void test_reuse_policy()
{
    ArenaFixture f( 16, 2 );

    // Relax window: freed in the first couple seconds → instant reuse.
    abi::edict_t *a = f.arena.alloc_edict( 1.0 );
    REQUIRE( a != nullptr );
    a->v.health = 99.0f;
    f.arena.free_edict( a, 1.0 ); // freetime 1.0 < 2.0
    abi::edict_t *b = f.arena.alloc_edict( 1.01 );
    CHECK( b == a );                  // same slot reused immediately
    CHECK_EQ( b->v.health, 0.0f );    // re-init zeroed the stale fields
    CHECK_EQ( f.arena.num_entities(), 3u );

    // Grace period: freed later → must age 0.5s before reuse.
    f.arena.free_edict( b, 5.0 ); // freetime 5.0 >= 2.0
    abi::edict_t *c = f.arena.alloc_edict( 5.2 ); // 0.2 elapsed — not eligible
    REQUIRE( c != nullptr );
    CHECK( c != b );
    CHECK_EQ( f.arena.index_of( c ), 3 );

    abi::edict_t *d = f.arena.alloc_edict( 5.6 ); // 0.6 elapsed — eligible
    CHECK( d == b );
}

static void test_exhaustion_returns_null()
{
    ArenaFixture f( 4, 2 );

    CHECK( f.arena.alloc_edict( 1.0 ) != nullptr ); // index 2
    CHECK( f.arena.alloc_edict( 1.0 ) != nullptr ); // index 3
    CHECK( f.arena.alloc_edict( 1.0 ) == nullptr ); // max_edicts hit
    CHECK_EQ( f.arena.num_entities(), 4u );
}

namespace {
int   g_release_calls = 0;
void *g_release_ed    = nullptr;

void record_release( void *ctx, abi::edict_t *ed )
{
    ++g_release_calls;
    g_release_ed = ed;
    *static_cast<int *>( ctx ) += 1;
}
} // namespace

static void test_private_data_lifecycle()
{
    ArenaFixture f( 16, 2 );
    int ctx_hits = 0;
    f.arena.set_private_releaser( &record_release, &ctx_hits );
    g_release_calls = 0;
    g_release_ed    = nullptr;

    abi::edict_t *e = f.arena.alloc_edict( 1.0 );
    REQUIRE( e != nullptr );

    void *pv = f.arena.alloc_private( e, 32 );
    REQUIRE( pv != nullptr );
    CHECK( e->pvPrivateData == pv );
    CHECK_EQ( static_cast<char *>( pv )[0], 0 ); // zeroed

    f.arena.free_edict( e, 1.0 );
    CHECK_EQ( g_release_calls, 1 );
    CHECK( g_release_ed == e );
    CHECK_EQ( ctx_hits, 1 );
    CHECK( e->pvPrivateData == nullptr );

    // No private data → releaser not called again.
    f.arena.free_private( e );
    CHECK_EQ( g_release_calls, 1 );
}

static void test_private_data_size_roundup()
{
    // sv_game.c:2955 — "(cb + 15) & ~15": shipped binary mods write past
    // their requested block, so the over-allocation is load-bearing.
    CHECK_EQ( sv::EdictArena::private_data_size( 0 ), 0u );
    CHECK_EQ( sv::EdictArena::private_data_size( 1 ), 16u );
    CHECK_EQ( sv::EdictArena::private_data_size( 16 ), 16u );
    CHECK_EQ( sv::EdictArena::private_data_size( 17 ), 32u );
    CHECK_EQ( sv::EdictArena::private_data_size( 240 ), 240u );
    CHECK_EQ( sv::EdictArena::private_data_size( 241 ), 256u );
}

static void test_offset_contract()
{
    ArenaFixture f( 16, 2 );

    abi::edict_t *e3 = f.arena.edict_num( 3 );
    REQUIRE( e3 != nullptr );
    CHECK_EQ( f.arena.offset_of( e3 ),
              static_cast<std::ptrdiff_t>( 3 * sizeof( abi::edict_t )));
    CHECK( f.arena.ent_of_offset( f.arena.offset_of( e3 )) == e3 );
    CHECK_EQ( f.arena.index_of( e3 ), 3 );

    CHECK( f.arena.edict_num( 99 ) == nullptr ); // out of range
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_alloc_init_state );
    RUN_TEST( test_free_scrubs_subset_leaves_stale );
    RUN_TEST( test_reuse_policy );
    RUN_TEST( test_exhaustion_returns_null );
    RUN_TEST( test_private_data_lifecycle );
    RUN_TEST( test_private_data_size_roundup );
    RUN_TEST( test_offset_contract );

    std::printf( "edict_arena: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
