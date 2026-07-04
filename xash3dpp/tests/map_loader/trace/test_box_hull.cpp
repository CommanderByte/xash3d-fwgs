// xash3dpp — BoxHull construction, hull selection + offset math (C8)
// Covers: the fixed six-node chain + PM_InitBoxHull plane layout, the
// PM_HullForBox distance assignment, the Minkowski expansion convention,
// PM_HullForBsp's usehull→BSP-hull remap and centering offset, world_hull
// views (incl. absent hulls), and a trace against the fixture world's
// clipnode chain.
// Legacy reference: pm_trace.c:64-105, :146-176, :406-409.

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/trace.hpp>
#include <xash3dpp/map_loader/world.hpp>

#include "../bsp/test_bsp_builder.hpp"

#include "../../test_helpers.hpp"

namespace ml = xash::map_loader;
using test_bsp::make_minimal_world;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

static ml::WorldData load_world()
{
    auto w = ml::load_world_data( make_minimal_world().build(), "t",
                                  ml::WorldLoadOptions{} );
    REQUIRE( w.has_value() );
    return std::move( *w );
}

static void test_box_hull_layout()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -1, -2, -3 }, { 4, 5, 6 } );

    REQUIRE( hull.clipnodes.size() == 6 );
    REQUIRE( hull.planes.size() == 6 );
    CHECK_EQ( hull.firstclipnode, 0 );
    CHECK_EQ( hull.lastclipnode, 5 );

    // PM_HullForBox distance layout.
    CHECK( hull.planes[0].dist == 4.0f );  // maxs.x
    CHECK( hull.planes[1].dist == -1.0f ); // mins.x
    CHECK( hull.planes[2].dist == 5.0f );  // maxs.y
    CHECK( hull.planes[3].dist == -2.0f ); // mins.y
    CHECK( hull.planes[4].dist == 6.0f );  // maxs.z
    CHECK( hull.planes[5].dist == -3.0f ); // mins.z

    // Plane types i>>1, unit axial normals, signbits 0.
    CHECK_EQ( hull.planes[0].type, std::uint8_t{ 0 } );
    CHECK_EQ( hull.planes[3].type, std::uint8_t{ 1 } );
    CHECK_EQ( hull.planes[5].type, std::uint8_t{ 2 } );
    CHECK( hull.planes[2].normal.y == 1.0f && hull.planes[2].normal.x == 0.0f );
    CHECK_EQ( hull.planes[4].signbits, std::uint8_t{ 0 } );

    // Chain shape (mod_bmodel.c BOX_CLIPNODES_INITIALIZER).
    CHECK_EQ( hull.clipnodes[0].children[0], ml::k_contents_empty );
    CHECK_EQ( hull.clipnodes[0].children[1], 1 );
    CHECK_EQ( hull.clipnodes[5].children[0], ml::k_contents_solid );
    CHECK_EQ( hull.clipnodes[5].children[1], ml::k_contents_empty );
}

static void test_minkowski_expansion_convention()
{
    // Sweeping a standing player (usehull 0: mins {-16,-16,-36},
    // maxs {16,16,36}) against an entity AABB {-8..8}^3:
    // expanded mins = ent.mins - player_maxs, maxs = ent.maxs - player_mins.
    const ml::HullBounds player = ml::k_default_hull_bounds[0];
    const Vec3 ent_mins{ -8, -8, -8 }, ent_maxs{ 8, 8, 8 };

    ml::BoxHull box;
    const auto &hull = box.set_bounds( ent_mins - player.maxs,
                                       ent_maxs - player.mins );

    CHECK( hull.planes[0].dist == 24.0f );  // 8 - (-16)
    CHECK( hull.planes[1].dist == -24.0f ); // -8 - 16
    CHECK( hull.planes[4].dist == 44.0f );  // 8 - (-36)
    CHECK( hull.planes[5].dist == -44.0f ); // -8 - 36

    // The player-center point at {0,0,40} is above the expanded box → empty;
    // at {0,0,40} vs z-max 44 → inside → solid.
    CHECK_EQ( ml::hull_point_contents( hull, 0, { 0, 0, 48 } ), ml::k_contents_empty );
    CHECK_EQ( ml::hull_point_contents( hull, 0, { 0, 0, 40 } ), ml::k_contents_solid );
}

static void test_hull_for_bsp_remap_and_offset()
{
    const auto w = load_world();

    // usehull 0 (standing) → BSP hull 1; default table: clip_mins matches
    // player mins → offset = origin.
    const auto standing = ml::hull_for_bsp( w, 0, 0, ml::k_default_hull_bounds[0],
                                            { 10, 20, 30 } );
    CHECK_EQ( standing.hull.firstclipnode, 0 );
    CHECK( standing.hull.clip_mins.z == -36.0f );
    CHECK( standing.offset.x == 10.0f && standing.offset.z == 30.0f );

    // usehull 2 (point) → BSP hull 0 (the MakeHull0 view).
    const auto point = ml::hull_for_bsp( w, 0, 2, ml::k_default_hull_bounds[2], {} );
    CHECK_EQ( point.hull.clipnodes.size(), w.hull0_nodes().size() );
    CHECK( point.hull.clip_mins.x == 0.0f );

    // usehull 1 (ducked) → BSP hull 3; fixture hull 3 has headnode -1 and
    // clip_mins from the head-hull slot.
    const auto ducked = ml::hull_for_bsp( w, 0, 1, ml::k_default_hull_bounds[1], {} );
    CHECK( ducked.hull.clip_mins.z == -18.0f );
    CHECK_EQ( ducked.hull.firstclipnode, -1 );

    // Mod-resized player bounds shift the centering offset
    // (offset = clip_mins - player_mins + origin).
    const ml::HullBounds resized{ { -12, -12, -30 }, { 12, 12, 30 } };
    const auto shifted = ml::hull_for_bsp( w, 0, 0, resized, {} );
    CHECK( shifted.offset.z == -6.0f ); // -36 - (-30)
}

static void test_world_hull_views_and_trace()
{
    const auto w = load_world();

    // Hull 1 walks the shared clipnodes; fixture chain: x>=128 empty,
    // then plane 1 (y = -64, normal -Y): solid below, empty above.
    const auto h1 = ml::world_hull( w, 0, 1 );
    REQUIRE( h1.clipnodes.size() == w.clipnodes().size() );
    CHECK_EQ( ml::hull_point_contents( h1, h1.firstclipnode, { 200, 0, 0 } ),
              ml::k_contents_empty );
    // x < 128 and -y > -64 (i.e. y < 64... plane {0,-1,0} dist -64:
    // diff = -y + 64 >= 0 for y <= 64) → children[0] = SOLID.
    CHECK_EQ( ml::hull_point_contents( h1, h1.firstclipnode, { 0, 0, 0 } ),
              ml::k_contents_solid );

    // Trace into the solid region hits the x-plane (node 0, plane 0).
    const auto tr = ml::trace_hull( h1, { 200, 0, 0 }, { 0, 0, 0 } );
    CHECK( !tr.allsolid );
    CHECK( tr.fraction < 1.0f );
    CHECK( tr.plane.normal.x == 1.0f );
    CHECK( tr.plane.dist == 128.0f );

    // Absent hull (world_hull on a missing descriptor) → CONTENTS_NONE and
    // fully-open traces.
    auto b = make_minimal_world();
    std::vector<xash::map_loader::bsp::dmodel_t> models( 1 );
    models[0] = { { -64, -64, -64 }, { 64, 64, 64 }, {}, { 0, 5, -1, -1 }, 2, 0, 2 };
    b.set_lump_records( xash::map_loader::bsp::k_lump_models, models );
    const auto w2r = ml::load_world_data( b.build(), "t", ml::WorldLoadOptions{} );
    REQUIRE( w2r.has_value() );

    const auto missing = ml::world_hull( *w2r, 0, 1 ); // ZHLT-skipped hull
    CHECK_EQ( ml::hull_point_contents( missing, missing.firstclipnode, { 0, 0, 0 } ),
              ml::k_contents_none );
    const auto tr2 = ml::trace_hull( missing, { 200, 0, 0 }, { 0, 0, 0 } );
    CHECK( tr2.inopen );
    CHECK( tr2.fraction == 1.0f );
}

int main()
{
    RUN_TEST( test_box_hull_layout );
    RUN_TEST( test_minkowski_expansion_convention );
    RUN_TEST( test_hull_for_bsp_remap_and_offset );
    RUN_TEST( test_world_hull_views_and_trace );

    std::printf( "box_hull: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
