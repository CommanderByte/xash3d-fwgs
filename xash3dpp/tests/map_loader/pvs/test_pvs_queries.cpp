// xash3dpp — PVS query tests (Chunk 5, C7)
// Covers: point_leaf incl. the on-plane BACK-child tie-break, pvs_for_point
// (cluster gating, visofs -1 → all-visible), leaf_compressed_pvs spans,
// box_leafnums (cluster collection, solid pruning, topnode) via
// BOX_ON_PLANE_SIDE, box_visible, and fat_pvs (radius split, merge,
// fullvis + clusterless fallbacks).
// Fixture geometry (make_minimal_world): one splitting plane x=128
// (type PLANE_X, normal +X); front child → leaf 1 (EMPTY, cluster 0,
// visofs 0), back child → leaf 2 (WATER, cluster 1, visofs -1);
// visdata = {0x03, 0x00, 0x01, 0xFF}, visbytes = 1.

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/map_loader/world.hpp>

#include "../bsp/test_bsp_builder.hpp"

#include "../../test_helpers.hpp"

#include <array>
#include <vector>

namespace ml = xash::map_loader;
using test_bsp::make_minimal_world;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

static ml::WorldData load_world( bool is_world = true )
{
    ml::WorldLoadOptions opts;
    opts.is_world = is_world;
    auto w = ml::load_world_data( make_minimal_world().build(), "t", opts );
    REQUIRE( w.has_value() );
    return std::move( *w );
}

// ---------------------------------------------------------------------------
// point_leaf
// ---------------------------------------------------------------------------

static void test_point_leaf_sides()
{
    const auto w = load_world();

    CHECK_EQ( ml::point_leaf( w, { 200.0f, 0.0f, 0.0f } ), 1 ); // front of x=128
    CHECK_EQ( ml::point_leaf( w, { 100.0f, 0.0f, 0.0f } ), 2 ); // behind
}

static void test_point_leaf_on_plane_tiebreak()
{
    const auto w = load_world();

    // Exactly on the plane: PlaneDiff == 0 → "<= 0" → BACK child (leaf 2).
    // This pins the asymmetry vs the hull walk's strict "< 0" (front); the
    // companion trace-side test lands with the C8 kernel.
    CHECK_EQ( ml::point_leaf( w, { 128.0f, 0.0f, 0.0f } ), 2 );
}

// ---------------------------------------------------------------------------
// leaf_compressed_pvs / pvs_for_point
// ---------------------------------------------------------------------------

static void test_leaf_compressed_pvs_spans()
{
    const auto w = load_world();

    const auto run1 = ml::leaf_compressed_pvs( w, 1 ); // visofs 0
    REQUIRE( run1.size() == 4 );                       // to the end of visdata
    CHECK_EQ( static_cast<unsigned>( run1[0] ), 0x03u );

    CHECK( ml::leaf_compressed_pvs( w, 2 ).empty() );  // visofs -1
    CHECK( ml::leaf_compressed_pvs( w, -1 ).empty() ); // invalid index
    CHECK( ml::leaf_compressed_pvs( w, 99 ).empty() );
}

static void test_pvs_for_point()
{
    const auto w = load_world();
    std::array<std::byte, 1> vis{};

    // Front point → leaf 1 (cluster 0, visofs 0) → literal 0x03.
    REQUIRE( ml::pvs_for_point( w, { 200.0f, 0.0f, 0.0f }, vis ));
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0x03u );

    // Back point → leaf 2 (cluster 1, visofs -1 → NULL) → all visible.
    REQUIRE( ml::pvs_for_point( w, { 100.0f, 0.0f, 0.0f }, vis ));
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0xFFu );

    // Non-world load: every cluster is -1 → legacy NULL → false.
    const auto bm = load_world( /*is_world=*/false );
    CHECK( !ml::pvs_for_point( bm, { 200.0f, 0.0f, 0.0f }, vis ));
}

// ---------------------------------------------------------------------------
// box_leafnums / box_visible
// ---------------------------------------------------------------------------

static void test_box_leafnums()
{
    const auto w = load_world();
    std::array<int, ml::k_max_box_leafs> list{};
    int topnode = -2;

    // Box straddling the plane → both leafs, clusters {0, 1}, topnode 0.
    auto n = ml::box_leafnums( w, { 100, -10, -10 }, { 200, 10, 10 }, list, &topnode );
    REQUIRE( n == 2 );
    CHECK_EQ( list[0], 0 );
    CHECK_EQ( list[1], 1 );
    CHECK_EQ( topnode, 0 );

    // Box fully in front → only cluster 0; no straddling node.
    n = ml::box_leafnums( w, { 150, -10, -10 }, { 200, 10, 10 }, list, &topnode );
    REQUIRE( n == 1 );
    CHECK_EQ( list[0], 0 );
    CHECK_EQ( topnode, -1 );

    // Overflow: a one-slot list stops after the first leaf.
    std::array<int, 1> tiny{};
    n = ml::box_leafnums( w, { 100, -10, -10 }, { 200, 10, 10 }, tiny, nullptr );
    CHECK_EQ( n, std::size_t{ 1 } );
}

static void test_box_visible()
{
    const auto w = load_world();

    const auto vis_c0 = std::array<std::byte, 1>{ std::byte{ 0x01 } }; // cluster 0 only
    const auto vis_c1 = std::array<std::byte, 1>{ std::byte{ 0x02 } }; // cluster 1 only

    // Front box touches only cluster 0.
    CHECK( ml::box_visible( w, { 150, -10, -10 }, { 200, 10, 10 }, vis_c0 ));
    CHECK( !ml::box_visible( w, { 150, -10, -10 }, { 200, 10, 10 }, vis_c1 ));

    // Straddling box touches both.
    CHECK( ml::box_visible( w, { 100, -10, -10 }, { 200, 10, 10 }, vis_c1 ));

    // Empty visbits → legacy NULL → true.
    CHECK( ml::box_visible( w, { 150, -10, -10 }, { 200, 10, 10 }, {} ));
}

// ---------------------------------------------------------------------------
// fat_pvs
// ---------------------------------------------------------------------------

static void test_fat_pvs()
{
    const auto w = load_world();
    std::array<std::byte, 1> vis{};

    // Far in front (d = 72 > radius 8): only leaf 1's row (0x03).
    auto n = ml::fat_pvs( w, { 200, 0, 0 }, ml::k_fatpvs_radius, vis, false, false );
    CHECK_EQ( n, std::size_t{ 1 } );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0x03u );

    // Near the plane (|d| <= radius): both leafs; leaf 2 has no vis row →
    // all-visible ORed in.
    n = ml::fat_pvs( w, { 130, 0, 0 }, ml::k_fatpvs_radius, vis, false, false );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0xFFu );

    // merge=true accumulates instead of clearing.
    vis[0] = std::byte{ 0x80 };
    n = ml::fat_pvs( w, { 200, 0, 0 }, ml::k_fatpvs_radius, vis, true, false );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0x83u );

    // fullvis → 0xFF regardless.
    vis[0] = std::byte{ 0 };
    n = ml::fat_pvs( w, { 200, 0, 0 }, ml::k_fatpvs_radius, vis, false, true );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0xFFu );

    // Clusterless point leaf (non-world load) → full visibility fallback.
    const auto bm = load_world( /*is_world=*/false );
    std::array<std::byte, 1> vis2{};
    n = ml::fat_pvs( bm, { 200, 0, 0 }, ml::k_fatpvs_radius, vis2, false, false );
    CHECK_EQ( n, std::size_t{ 0 } ); // non-world: visbytes == 0
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_point_leaf_sides );
    RUN_TEST( test_point_leaf_on_plane_tiebreak );
    RUN_TEST( test_leaf_compressed_pvs_spans );
    RUN_TEST( test_pvs_for_point );
    RUN_TEST( test_box_leafnums );
    RUN_TEST( test_box_visible );
    RUN_TEST( test_fat_pvs );

    std::printf( "pvs_queries: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
