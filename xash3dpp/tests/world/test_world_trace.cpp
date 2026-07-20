// xash3dpp — server trace composition pins (Chunk 6 S5b)
// Covers: SV_HullForBsp size selection (point-hull verbatim-clip_mins
// quirk, portal force, hull-1 pick), SV_HullForEntity guards + Minkowski
// boxes, world clipping through the real kernel (hit fraction, plane,
// ent stamp), entity box clipping, the SV_Move world-first fraction
// re-compose quirk, filter chain pins (passedict, owner, NOMONSTERS vs
// PUSHSTEP, SOLID_NOT), the rotated-entity path (360° identity
// equivalence), and the transformed-plane containment invariant.
// Fixture: make_minimal_world — hull 1 solid region is x<128 && y<64;
// hull 1 clip bounds are the standard HL {-16,-16,-36}/{16,16,36}.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/world/trace.hpp>
#include <xash3dpp/utilities/matrix.hpp>

#include "../map_loader/bsp/test_bsp_builder.hpp"

#include "../test_helpers.hpp"

#include <cmath>
#include <optional>

namespace sv  = xash::server;
namespace wr  = ::xash::world;
namespace abi = xash::abi;
namespace ml  = xash::map_loader;
namespace ut  = xash::utilities;
using ut::Vec3;

static int g_pass = 0, g_fail = 0;

namespace {

struct FixtureResolver final : wr::IModelResolver
{
    std::optional<wr::BrushModel> brush_model( int modelindex ) noexcept override
    {
        if ( modelindex == 1 )
            return wr::BrushModel{ 0 }; // the world model
        return std::nullopt;
    }
    bool is_studio( int ) noexcept override { return false; }
};

struct LinkHooks final : wr::IWorldLinkHooks
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

struct TraceFixture
{
    xash::memory::PoolHandle     pool;
    sv::EdictArena               arena;
    wr::WorldLinks               links;
    LinkHooks                    link_hooks;
    FixtureResolver              resolver;
    std::optional<ml::WorldData> world;
    wr::LinkEnv                  lenv;
    wr::MoveEnv                  env;

    TraceFixture()
    {
        pool = xash::memory::create_pool( "test_world_trace" );
        REQUIRE( static_cast<bool>( pool ));
        REQUIRE( arena.init( pool, 32, 2 ));

        ml::WorldLoadOptions opts;
        opts.is_world = true;
        auto w = ml::load_world_data( test_bsp::make_minimal_world().build(),
                                      "t", opts );
        REQUIRE( w.has_value() );
        world.emplace( std::move( *w ));

        links.set_hooks( &link_hooks );
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

    ~TraceFixture()
    {
        arena.shutdown();
        xash::memory::destroy_pool( pool );
    }

    abi::edict_t *spawn_box( Vec3 origin, float half, int solid )
    {
        abi::edict_t *e = arena.alloc_edict( 1.0 );
        REQUIRE( e != nullptr );
        sv::store_vec3( e->v.origin, origin );
        sv::store_vec3( e->v.mins, { -half, -half, -half } );
        sv::store_vec3( e->v.maxs, { half, half, half } );
        sv::store_vec3( e->v.size, { 2 * half, 2 * half, 2 * half } );
        e->v.solid = solid;
        links.link_edict( e, false, lenv );
        return e;
    }
};

// Human-ish probe box that selects world hull 1 (size.x 32 <= 36,
// size.z 80 > 36).
const Vec3 k_h1_mins = { -16.0f, -16.0f, -40.0f };
const Vec3 k_h1_maxs = { 16.0f, 16.0f, 40.0f };

} // namespace

// ---------------------------------------------------------------------------
// hull selection
// ---------------------------------------------------------------------------

static void test_hull_for_bsp_selection()
{
    TraceFixture f;

    // Point probe → hull 0, offset = clip_mins VERBATIM + origin.
    auto h0 = wr::hull_for_bsp_entity( f.env, f.env.worldspawn, {}, {} );
    REQUIRE( h0.has_value() );
    CHECK_EQ( h0->offset.x, h0->hull.clip_mins.x );
    CHECK_EQ( h0->offset.z, h0->hull.clip_mins.z );

    // Human-size probe → hull 1 (standard HL clip bounds), offset =
    // clip_mins - mins.
    auto h1 = wr::hull_for_bsp_entity( f.env, f.env.worldspawn,
                                       k_h1_mins, k_h1_maxs );
    REQUIRE( h1.has_value() );
    REQUIRE( !h1->hull.clipnodes.empty() );
    CHECK_EQ( h1->hull.clip_mins.z, -36.0f );
    CHECK_EQ( h1->offset.x, 0.0f );  // -16 - (-16)
    CHECK_EQ( h1->offset.z, 4.0f );  // -36 - (-40)

    // Non-brush model → the legacy Host_Error condition → nullopt.
    abi::edict_t *bogus = f.arena.alloc_edict( 1.0 );
    bogus->v.modelindex = 99;
    bogus->v.solid      = abi::k_solid_bsp;
    bogus->v.movetype   = abi::k_movetype_push;
    CHECK( !wr::hull_for_bsp_entity( f.env, bogus, {}, {} ).has_value() );
}

static void test_hull_for_entity()
{
    TraceFixture f;

    // SOLID_BSP without PUSH/PUSHSTEP → guard trips.
    abi::edict_t *bad = f.arena.alloc_edict( 1.0 );
    bad->v.modelindex = 1;
    bad->v.solid      = abi::k_solid_bsp;
    bad->v.movetype   = 0;
    ml::BoxHull storage;
    CHECK( !wr::hull_for_entity( f.env, bad, {}, {}, storage ).has_value() );

    // Box solid → Minkowski expansion, offset = origin.
    abi::edict_t *box = f.spawn_box( { 200, 100, 50 }, 8.0f,
                                     abi::k_solid_bbox );
    const Vec3 probe_mins = { -4, -4, -4 }, probe_maxs = { 4, 4, 4 };
    auto h = wr::hull_for_entity( f.env, box, probe_mins, probe_maxs,
                                  storage );
    REQUIRE( h.has_value() );
    CHECK_EQ( h->offset.x, 200.0f );
    // Minkowski expansion (ent ±8 grown by probe ±4 → ±12) verified
    // behaviourally: local points just inside/outside the expanded box.
    CHECK_EQ( ml::hull_point_contents( h->hull, h->hull.firstclipnode,
                                       { 11.0f, 0.0f, 0.0f } ),
              ml::k_contents_solid );
    CHECK_EQ( ml::hull_point_contents( h->hull, h->hull.firstclipnode,
                                       { 13.0f, 0.0f, 0.0f } ),
              ml::k_contents_empty );
}

// ---------------------------------------------------------------------------
// world clipping + SV_Move composition
// ---------------------------------------------------------------------------

static void test_world_clip_hit()
{
    TraceFixture f;

    // Open-region trace (x > 128 stays empty in hull 1): clean miss.
    auto clear = wr::move( f.env, { 200, 0, 50 }, k_h1_mins, k_h1_maxs,
                           { 200, 100, 50 }, abi::k_move_normal, nullptr,
                           false );
    CHECK_EQ( clear.t.fraction, 1.0f );
    CHECK( clear.ent == nullptr );
    CHECK_EQ( clear.t.endpos.y, 100.0f );

    // Into the x<128 solid: hit near the plane, +X normal, world stamped.
    auto hit = wr::move( f.env, { 200, 0, 50 }, k_h1_mins, k_h1_maxs,
                         { 0, 0, 50 }, abi::k_move_normal, nullptr, false );
    CHECK( hit.t.fraction > 0.2f && hit.t.fraction < 0.5f );
    CHECK( hit.ent == f.env.worldspawn );
    CHECK_EQ( hit.t.plane.normal.x, 1.0f );
    CHECK( hit.t.endpos.x > 128.0f && hit.t.endpos.x < 160.0f );
}

static void test_entity_box_clip_and_rescale()
{
    TraceFixture f;

    // Box in the open region, in front of the world wall along the ray.
    // The h1 probe's Minkowski face sits at 150 + 8 + 16 = 174.
    abi::edict_t *box = f.spawn_box( { 150, 0, 50 }, 8.0f,
                                     abi::k_solid_bbox );

    // The returned fraction is entity-fraction × world-fraction (the
    // re-compose quirk) — equal to the direct distance ratio.
    auto tr = wr::move( f.env, { 200, 0, 50 }, k_h1_mins, k_h1_maxs,
                        { 0, 0, 50 }, abi::k_move_normal, nullptr, false );
    CHECK( tr.ent == box );
    CHECK( std::fabs( tr.t.fraction - ( 200.0f - 174.0f ) / 200.0f ) < 0.01f );

    // Box BEHIND the world wall is never reached; the world hit stands.
    wr::WorldLinks::unlink_edict( box );
    abi::edict_t *hidden = f.spawn_box( { 60, 0, 50 }, 8.0f,
                                        abi::k_solid_bbox );
    auto tr2 = wr::move( f.env, { 200, 0, 50 }, k_h1_mins, k_h1_maxs,
                         { 0, 0, 50 }, abi::k_move_normal, nullptr, false );
    CHECK( tr2.ent == f.env.worldspawn );
    CHECK( hidden->v.solid == abi::k_solid_bbox ); // untouched
}

static void test_clip_filters()
{
    TraceFixture f;

    abi::edict_t *box = f.spawn_box( { 150, 0, 50 }, 8.0f,
                                     abi::k_solid_bbox );

    // passedict skips itself.
    auto self = wr::move( f.env, { 250, 0, 50 }, {}, {}, { 200, 0, 50 },
                          abi::k_move_normal, box, false );
    CHECK( self.ent == nullptr );

    // Owner symmetry: a missile never clips its owner (and vice versa).
    abi::edict_t *missile = f.spawn_box( { 250, 0, 50 }, 1.0f,
                                         abi::k_solid_bbox );
    missile->v.owner = box;
    auto own = wr::move( f.env, { 250, 0, 50 }, {}, {}, { 140, 0, 50 },
                         abi::k_move_normal, missile, false );
    CHECK( own.ent == nullptr );
    wr::WorldLinks::unlink_edict( missile ); // out of later traces' way

    // MOVE_NOMONSTERS skips ordinary solids...
    auto nomon = wr::move( f.env, { 250, 0, 50 }, {}, {}, { 140, 0, 50 },
                           abi::k_move_nomonsters, nullptr, false );
    CHECK( nomon.ent == nullptr );

    // ...but MOVETYPE_PUSHSTEP pushables still clip.
    box->v.movetype = abi::k_movetype_pushstep;
    auto push = wr::move( f.env, { 250, 0, 50 }, {}, {}, { 140, 0, 50 },
                          abi::k_move_nomonsters, nullptr, false );
    CHECK( push.ent == box );

    // SOLID_NOT never clips.
    box->v.movetype = 0;
    box->v.solid    = abi::k_solid_not;
    auto ghost = wr::move( f.env, { 250, 0, 50 }, {}, {}, { 140, 0, 50 },
                           abi::k_move_normal, nullptr, false );
    CHECK( ghost.ent == nullptr );
}

// ---------------------------------------------------------------------------
// rotated path
// ---------------------------------------------------------------------------

static void test_rotated_identity_equivalence()
{
    TraceFixture f;

    // A full-turn angle forces the rotated code path with an identity
    // rotation — the result must match the unrotated trace.
    abi::edict_t *ws = f.env.worldspawn;

    auto straight = wr::clip_move_to_entity( f.env, ws, { 200, 0, 50 },
                                             k_h1_mins, k_h1_maxs,
                                             { 0, 0, 50 } );

    ws->v.angles[1] = 360.0f;
    auto rotated = wr::clip_move_to_entity( f.env, ws, { 200, 0, 50 },
                                            k_h1_mins, k_h1_maxs,
                                            { 0, 0, 50 } );
    ws->v.angles[1] = 0.0f;

    REQUIRE( straight.t.fraction < 1.0f );
    CHECK( std::fabs( rotated.t.fraction - straight.t.fraction ) < 1e-3f );
    CHECK( std::fabs( rotated.t.plane.normal.x -
                      straight.t.plane.normal.x ) < 1e-3f );
    CHECK( std::fabs( rotated.t.plane.dist - straight.t.plane.dist ) < 0.1f );
}

static void test_transform_positive_plane_invariant()
{
    // The transformed plane must contain the transformed on-plane point,
    // and keep a unit normal (convention-free correctness check).
    const ut::Matrix3x4 m =
        ut::from_angles( { 10.0f, -3.0f, 7.0f }, { 0.0f, 90.0f, 0.0f } );

    const ml::TracePlane in = { { 1.0f, 0.0f, 0.0f }, 5.0f };
    ml::TracePlane out;
    wr::transform_positive_plane( m, in, out );

    const Vec3 p_world = ut::transform_point( m, { 5.0f, 0.0f, 0.0f } );
    CHECK( std::fabs( ut::dot( out.normal, p_world ) - out.dist ) < 1e-3f );
    CHECK( std::fabs( ut::length( out.normal ) - 1.0f ) < 1e-4f );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_hull_for_bsp_selection );
    RUN_TEST( test_hull_for_entity );
    RUN_TEST( test_world_clip_hit );
    RUN_TEST( test_entity_box_clip_and_rescale );
    RUN_TEST( test_clip_filters );
    RUN_TEST( test_rotated_identity_equivalence );
    RUN_TEST( test_transform_positive_plane_invariant );

    std::printf( "world_trace: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
