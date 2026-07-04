// xash3dpp — point contents, water links, lightstyles pins (Chunk 6 S5c)
// Covers: RankForContents goldens, world hull-0 contents, SOLID_NOT water
// bmodel merging (rank priority, group-mask gate), CURRENT_* folding, the
// exact brush-trigger hull test (hit / miss / non-brush passthrough), and
// lightstyle storage semantics (pattern map, reset value, bounds) + the
// light_for_entity guards incl. the documented no-lightdata stub.
// Fixture: make_minimal_world — hull 0 is WATER for x<128, EMPTY beyond;
// hull 1 is SOLID for x<128 && y<64.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/lightstyles.hpp>
#include <xash3dpp/private/server/world_links.hpp>
#include <xash3dpp/private/server/world_trace.hpp>

#include "../../map_loader/bsp/test_bsp_builder.hpp"

#include "../../test_helpers.hpp"

#include <optional>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace ml  = xash::map_loader;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

namespace {

struct FixtureResolver final : sv::IModelResolver
{
    std::optional<sv::BrushModel> brush_model( int modelindex ) noexcept override
    {
        if ( modelindex == 1 )
            return sv::BrushModel{ 0 };
        return std::nullopt;
    }
    bool is_studio( int ) noexcept override { return false; }
};

struct LinkHooks final : sv::IWorldLinkHooks
{
    void set_abs_box( abi::edict_t *ent ) noexcept override
    {
        sv::EntityView v( ent );
        const Vec3 org = v.origin();
        v.set_absmin( org + v.mins() );
        v.set_absmax( org + v.maxs() );
    }
    void dispatch_touch( abi::edict_t *, abi::edict_t * ) noexcept override {}
};

struct ContentsFixture
{
    xash::memory::PoolHandle     pool;
    sv::EdictArena               arena;
    sv::WorldLinks               links;
    LinkHooks                    hooks;
    FixtureResolver              resolver;
    std::optional<ml::WorldData> world;
    sv::LinkEnv                  lenv;
    sv::MoveEnv                  env;

    ContentsFixture()
    {
        pool = xash::memory::create_pool( "test_world_contents" );
        REQUIRE( static_cast<bool>( pool ));
        REQUIRE( arena.init( pool, 32, 2 ));

        ml::WorldLoadOptions opts;
        opts.is_world = true;
        auto w = ml::load_world_data( test_bsp::make_minimal_world().build(),
                                      "t", opts );
        REQUIRE( w.has_value() );
        world.emplace( std::move( *w ));

        links.set_hooks( &hooks );
        links.clear_world( { -256, -256, -256 }, { 256, 256, 256 } );

        abi::edict_t *ws = arena.edict_num( 0 );
        ws->v.modelindex = 1;
        ws->v.solid      = abi::k_solid_bsp;
        ws->v.movetype   = abi::k_movetype_push;

        lenv.world      = &*world;
        lenv.worldspawn = ws;

        env.world      = &*world;
        env.models     = &resolver;
        env.area_root  = links.root();
        env.worldspawn = ws;
    }

    ~ContentsFixture()
    {
        arena.shutdown();
        xash::memory::destroy_pool( pool );
    }

    // SOLID_NOT water bmodel reusing the world brush at `origin`.
    abi::edict_t *spawn_water( Vec3 origin, int skin )
    {
        abi::edict_t *e = arena.alloc_edict( 1.0 );
        REQUIRE( e != nullptr );
        sv::store_vec3( e->v.origin, origin );
        sv::store_vec3( e->v.mins, { -256, -256, -256 } );
        sv::store_vec3( e->v.maxs, { 256, 256, 256 } );
        e->v.solid      = abi::k_solid_not;
        e->v.skin       = skin;
        e->v.modelindex = 1;
        links.link_edict( e, false, lenv );
        return e;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// contents
// ---------------------------------------------------------------------------

static void test_rank_goldens()
{
    CHECK_EQ( sv::rank_for_contents( ml::k_contents_empty ), 0 );
    CHECK_EQ( sv::rank_for_contents( ml::k_contents_water ), 1 );
    CHECK_EQ( sv::rank_for_contents( ml::k_contents_slime ), 9 );
    CHECK_EQ( sv::rank_for_contents( ml::k_contents_solid ), 12 );
    CHECK_EQ( sv::rank_for_contents( 42 ), 13 ); // user contents win
}

static void test_world_point_contents()
{
    ContentsFixture f;

    CHECK_EQ( sv::true_point_contents( f.env, { 200, 0, 0 } ),
              ml::k_contents_empty );
    CHECK_EQ( sv::true_point_contents( f.env, { 100, 0, 0 } ),
              ml::k_contents_water );
}

static void test_water_entity_merge()
{
    ContentsFixture f;

    // Slime bmodel parked in the open region: its local hull-0 WATER zone
    // covers local x<128 → world x<428 at origin 300.
    f.spawn_water( { 300, 0, 0 }, ml::k_contents_slime );

    // Inside the ent's non-empty hull region: slime outranks empty.
    CHECK_EQ( sv::true_point_contents( f.env, { 310, 0, 0 } ),
              ml::k_contents_slime );

    // Beyond the ent's water zone: plain world empty.
    CHECK_EQ( sv::true_point_contents( f.env, { 440, 200, 0 } ),
              ml::k_contents_empty );
}

static void test_group_mask_gate_and_current_fold()
{
    ContentsFixture f;

    abi::edict_t *cur = f.spawn_water( { 300, 0, 0 },
                                       ml::k_contents_current_90 );
    cur->v.groupinfo = 0x4;

    // AND policy with a non-overlapping mask skips the volume entirely.
    f.env.group_mask = 0x1;
    CHECK_EQ( sv::point_contents( f.env, { 310, 0, 0 } ),
              ml::k_contents_empty );

    // Overlapping mask: CURRENT_90 folds to WATER in point_contents...
    f.env.group_mask = 0x4;
    CHECK_EQ( sv::point_contents( f.env, { 310, 0, 0 } ),
              ml::k_contents_water );

    // ...but true_point_contents reports the raw current.
    CHECK_EQ( sv::true_point_contents( f.env, { 310, 0, 0 } ),
              ml::k_contents_current_90 );
}

// ---------------------------------------------------------------------------
// brush trigger refinement
// ---------------------------------------------------------------------------

static void test_brush_trigger_exact_test()
{
    ContentsFixture f;

    // Trigger reusing the world brush at 300: its hull-1 SOLID zone (for
    // a human-sized toucher) is local x<128 && y<64.
    abi::edict_t *trig = f.arena.alloc_edict( 1.0 );
    sv::store_vec3( trig->v.origin, { 300, 0, 0 } );
    trig->v.solid      = abi::k_solid_trigger;
    trig->v.modelindex = 1;

    abi::edict_t *toucher = f.arena.alloc_edict( 1.0 );
    sv::store_vec3( toucher->v.mins, { -16, -16, -40 } );
    sv::store_vec3( toucher->v.maxs, { 16, 16, 40 } );

    // Toucher origin inside the trigger's solid hull → touch confirmed.
    sv::store_vec3( toucher->v.origin, { 310, 0, 46 } );
    CHECK( sv::brush_trigger_intersects( f.env, trig, toucher ));

    // Outside (local x > 128) → AABB hit rejected.
    sv::store_vec3( toucher->v.origin, { 450, 0, 46 } );
    CHECK( !sv::brush_trigger_intersects( f.env, trig, toucher ));

    // Non-brush trigger model → AABB verdict stands.
    trig->v.modelindex = 99;
    CHECK( sv::brush_trigger_intersects( f.env, trig, toucher ));
}

// ---------------------------------------------------------------------------
// lightstyles
// ---------------------------------------------------------------------------

static void test_lightstyles()
{
    sv::LightStyles styles;

    // Reset state: full value everywhere.
    CHECK_EQ( styles.style( 0 )->value, 256.0f );
    CHECK_EQ( styles.style( 255 )->length, 0 );
    CHECK( styles.style( 256 ) == nullptr );  // bounds
    CHECK( styles.style( -1 ) == nullptr );

    // 'a'-relative pattern map (SV_SetLightStyle).
    REQUIRE( styles.set( 3, "am", 1.5f ));
    CHECK_EQ( styles.style( 3 )->length, 2 );
    CHECK_EQ( styles.style( 3 )->map[0], 0.0f );
    CHECK_EQ( styles.style( 3 )->map[1], 12.0f );
    CHECK_EQ( styles.style( 3 )->time, 1.5f );

    CHECK( !styles.set( 999, "a", 0.0f )); // hardening
}

static void test_light_for_entity_guards()
{
    ContentsFixture f;

    // Invalid / freed → -1.
    CHECK_EQ( sv::light_for_entity( nullptr ), -1 );
    abi::edict_t *dead = f.arena.alloc_edict( 1.0 );
    f.arena.free_edict( dead, 1.0 );
    CHECK_EQ( sv::light_for_entity( dead ), -1 );

    // Fullbright effect → 255.
    abi::edict_t *bright = f.arena.alloc_edict( 1.0 );
    bright->v.effects = abi::k_ef_fullbright;
    CHECK_EQ( sv::light_for_entity( bright ), 255 );

    // Documented stub: without LUMP_LIGHTING every entity (players
    // included) takes the legacy no-lightdata branch → 255.
    abi::edict_t *plain = f.arena.alloc_edict( 1.0 );
    CHECK_EQ( sv::light_for_entity( plain ), 255 );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_rank_goldens );
    RUN_TEST( test_world_point_contents );
    RUN_TEST( test_water_entity_merge );
    RUN_TEST( test_group_mask_gate_and_current_fold );
    RUN_TEST( test_brush_trigger_exact_test );
    RUN_TEST( test_lightstyles );
    RUN_TEST( test_light_for_entity_guards );

    std::printf( "world_contents: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
