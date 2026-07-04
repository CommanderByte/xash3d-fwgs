// xash3dpp — core lump loading → WorldData (Chunk 5, C4)
// Covers: entities raw copy + worldspawn wad/message scan, plane signbits,
// submodel bounds spread + empty-bounds reset, world/bmodel leaf cluster
// derivation, raw (unclamped) visofs, leaf-0-solid enforcement, node child
// semantics + validation, marksurface fix-up + validation, required-lump
// presence, BSP2 record widths end-to-end.
// Legacy reference: mod_bmodel.c Mod_Load{Entities,Planes,Submodels,
// Visibility,MarkSurfaces,Leafs,Nodes}.

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/map_loader/bsp/bsp_loader.hpp>

#include "test_bsp_builder.hpp"

#include "../../test_helpers.hpp"

#include <string>
#include <vector>

namespace bsp = xash::map_loader::bsp;
namespace ml  = xash::map_loader;
using test_bsp::make_minimal_world;
using xash::core::ErrorCode;

static int g_pass = 0, g_fail = 0;

static ml::WorldLoadOptions world_opts()  { return ml::WorldLoadOptions{}; }
static ml::WorldLoadOptions bmodel_opts() { ml::WorldLoadOptions o; o.is_world = false; return o; }

// ---------------------------------------------------------------------------
// happy path: entities / planes / submodels / visdata
// ---------------------------------------------------------------------------

static void test_minimal_world_loads()
{
    const auto file = make_minimal_world().build();
    const auto w    = ml::load_world_data( file, "maps/test.bsp", world_opts() );
    REQUIRE( w.has_value() );

    CHECK( w->version() == ml::BspVersion::HalfLife );
    CHECK( w->name() == "maps/test.bsp" );

    CHECK( w->entities().find( "worldspawn" ) != std::string_view::npos );
    CHECK( w->wadlist() == "\\half-life\\valve\\halflife.wad;decals.wad" );
    CHECK( w->message() == "Test Map" );

    CHECK_EQ( w->visdata().size(), std::size_t{ 4 } );
    CHECK_EQ( static_cast<unsigned>( w->visdata()[0] ), 0x03u );
}

static void test_plane_signbits()
{
    const auto file = make_minimal_world().build();
    const auto w    = ml::load_world_data( file, "t", world_opts() );
    REQUIRE( w.has_value() );

    const auto planes = w->planes();
    REQUIRE( planes.size() == 2 );
    CHECK_EQ( planes[0].signbits, std::uint8_t{ 0 } );
    CHECK_EQ( planes[0].type, std::uint8_t{ bsp::k_plane_x } );
    CHECK( planes[0].dist == 128.0f );
    CHECK_EQ( planes[1].signbits, std::uint8_t{ 0b010 } ); // -Y normal
    CHECK( planes[1].normal.y == -1.0f );
}

static void test_submodel_bounds_spread()
{
    {
        const auto file = make_minimal_world().build();
        const auto w    = ml::load_world_data( file, "t", world_opts() );
        REQUIRE( w.has_value() );
        REQUIRE( w->submodels().size() == 1 );
        const auto &m = w->submodels()[0];
        CHECK( m.mins.x == -65.0f && m.maxs.x == 65.0f ); // ±1 spread
        CHECK_EQ( m.visleafs, 2 );
        CHECK_EQ( m.headnode[0], 0 );
        CHECK_EQ( m.headnode[1], -1 );
        CHECK_EQ( m.numfaces, 2 );
    }
    // Empty-bounds reset: 999999/-999999 → 0, then spread.
    {
        auto b = make_minimal_world();
        std::vector<bsp::dmodel_t> models( 1 );
        models[0] = { { 999999.0f, 999999.0f, 999999.0f },
                      { -999999.0f, -999999.0f, -999999.0f },
                      { 0, 0, 0 }, { 0, -1, -1, -1 }, 2, 0, 2 };
        b.set_lump_records( bsp::k_lump_models, models );
        const auto w = ml::load_world_data( b.build(), "t", world_opts() );
        REQUIRE( w.has_value() );
        const auto &m = w->submodels()[0];
        CHECK( m.mins.x == -1.0f && m.maxs.x == 1.0f );
    }
}

// ---------------------------------------------------------------------------
// leafs
// ---------------------------------------------------------------------------

static void test_leaf_clusters_world()
{
    const auto file = make_minimal_world().build();
    const auto w    = ml::load_world_data( file, "t", world_opts() );
    REQUIRE( w.has_value() );

    const auto leafs = w->leafs();
    REQUIRE( leafs.size() == 3 );

    CHECK_EQ( leafs[0].cluster, -1 ); // solid leaf 0 has no visdata
    CHECK_EQ( leafs[1].cluster, 0 );
    CHECK_EQ( leafs[2].cluster, 1 );

    // visofs kept raw and unclamped (legacy parity).
    CHECK_EQ( leafs[0].visofs, -1 );
    CHECK_EQ( leafs[1].visofs, 0 );
    CHECK_EQ( leafs[2].visofs, -1 );

    CHECK_EQ( leafs[0].contents, ml::k_contents_solid );
    CHECK_EQ( leafs[2].contents, ml::k_contents_water );
    CHECK_EQ( leafs[0].ambient_sound_level[0], std::uint8_t{ 1 } );
    CHECK_EQ( leafs[0].ambient_sound_level[3], std::uint8_t{ 4 } );
    CHECK_EQ( leafs[1].nummarksurfaces, 2 );

    CHECK_EQ( w->visclusters(), 2 );
    CHECK_EQ( w->visbytes(), std::size_t{ 1 } ); // (2+7)>>3
}

static void test_leaf_clusters_bmodel()
{
    const auto file = make_minimal_world().build();
    const auto w    = ml::load_world_data( file, "t", bmodel_opts() );
    REQUIRE( w.has_value() );

    for ( const auto &leaf : w->leafs() )
        CHECK_EQ( leaf.cluster, -1 ); // no visclusters on bmodels
    CHECK_EQ( w->visclusters(), 0 );
    CHECK_EQ( w->visbytes(), std::size_t{ 0 } );

    // Worldspawn scan is world-only.
    CHECK( w->wadlist().empty() );
}

static void test_leaf0_not_solid_fails()
{
    auto b = make_minimal_world();
    std::vector<bsp::dleaf_t> leafs( 3 );
    leafs[0] = { -1, -1, {}, {}, 0, 0, {} }; // EMPTY, must be SOLID
    leafs[1] = { -1,  0, {}, {}, 0, 0, {} };
    leafs[2] = { -3, -1, {}, {}, 0, 0, {} };
    b.set_lump_records( bsp::k_lump_leafs, leafs );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( !w.has_value() );
    CHECK( w.error() == ErrorCode::BspBadWorld );

    // Non-world models skip the check.
    const auto bm = ml::load_world_data( b.build(), "t", bmodel_opts() );
    CHECK( bm.has_value() );
}

// ---------------------------------------------------------------------------
// nodes
// ---------------------------------------------------------------------------

static void test_nodes_loaded()
{
    const auto file = make_minimal_world().build();
    const auto w    = ml::load_world_data( file, "t", world_opts() );
    REQUIRE( w.has_value() );

    REQUIRE( w->nodes().size() == 1 );
    const auto &n = w->nodes()[0];
    CHECK_EQ( n.planenum, 0 );
    CHECK_EQ( n.children[0], -2 ); // leaf index 1 (disk semantics: -1 - c)
    CHECK_EQ( n.children[1], -3 ); // leaf index 2
    CHECK( n.mins.x == -128.0f && n.maxs.z == 128.0f );
    CHECK_EQ( n.numsurfaces, 2 );
}

static void test_node_child_validation()
{
    auto b = make_minimal_world();
    std::vector<bsp::dnode_t> nodes( 1 );
    nodes[0] = { 0, { 5, -3 }, {}, {}, 0, 0 }; // node index 5 out of range
    b.set_lump_records( bsp::k_lump_nodes, nodes );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( !w.has_value() );
    CHECK( w.error() == ErrorCode::BspCorruptLump );
}

// ---------------------------------------------------------------------------
// marksurfaces
// ---------------------------------------------------------------------------

static void test_marksurface_fixup()
{
    auto b = make_minimal_world();
    const std::vector<bsp::dmarkface_t> marks = { 0xFFFF, 1 }; // (int16)-1 → fix-up
    b.set_lump_records( bsp::k_lump_marksurfaces, marks );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    REQUIRE( w->marksurfaces().size() == 2 );
    CHECK_EQ( w->marksurfaces()[0], 0 ); // remapped to surface 0
    CHECK_EQ( w->marksurfaces()[1], 1 );
}

static void test_marksurface_out_of_range_fails()
{
    auto b = make_minimal_world();
    const std::vector<bsp::dmarkface_t> marks = { 5, 0 }; // only 2 faces exist
    b.set_lump_records( bsp::k_lump_marksurfaces, marks );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( !w.has_value() );
    CHECK( w.error() == ErrorCode::BspCorruptLump );
}

// ---------------------------------------------------------------------------
// structural failures
// ---------------------------------------------------------------------------

static void test_missing_required_lump()
{
    // Rebuild the minimal world WITHOUT planes.
    auto b = make_minimal_world();
    test_bsp::TestBspBuilder stripped{ bsp::k_hlbsp_version };
    stripped.set_entities( test_bsp::k_minimal_world_entities );
    // (leafs/nodes/models only — no planes lump at all)
    std::vector<bsp::dleaf_t> leafs( 3 );
    leafs[0] = { -2, -1, {}, {}, 0, 0, {} };
    leafs[1] = { -1, -1, {}, {}, 0, 0, {} };
    leafs[2] = { -3, -1, {}, {}, 0, 0, {} };
    stripped.set_lump_records( bsp::k_lump_leafs, leafs );
    std::vector<bsp::dnode_t> nodes( 1 );
    nodes[0] = { 0, { -2, -3 }, {}, {}, 0, 0 };
    stripped.set_lump_records( bsp::k_lump_nodes, nodes );
    std::vector<bsp::dmodel_t> models( 1 );
    models[0] = { { -64, -64, -64 }, { 64, 64, 64 }, {}, { 0, -1, -1, -1 }, 2, 0, 0 };
    stripped.set_lump_records( bsp::k_lump_models, models );

    const auto w = ml::load_world_data( stripped.build(), "t", world_opts() );
    REQUIRE( !w.has_value() );
    // Nodes reference plane 0 which does not exist → caught as corrupt
    // before the finalize presence check.
    CHECK( w.error() == ErrorCode::BspCorruptLump );
}

static void test_malformed_worldspawn()
{
    auto b = make_minimal_world();
    b.set_entities( "not_a_brace padding padding padding padding" );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( !w.has_value() );
    CHECK( w.error() == ErrorCode::BspBadWorld );

    // Non-world loads skip the scan entirely.
    const auto bm = ml::load_world_data( b.build(), "t", bmodel_opts() );
    CHECK( bm.has_value() );
}

// ---------------------------------------------------------------------------
// BSP2 end-to-end
// ---------------------------------------------------------------------------

static void test_bsp2_world_loads()
{
    const auto file = make_minimal_world( /*bsp2=*/true ).build();
    const auto w    = ml::load_world_data( file, "t", world_opts() );
    REQUIRE( w.has_value() );

    CHECK( w->version() == ml::BspVersion::Bsp2 );
    REQUIRE( w->leafs().size() == 3 );
    CHECK_EQ( w->leafs()[0].contents, ml::k_contents_solid );
    CHECK_EQ( w->leafs()[1].cluster, 0 );
    REQUIRE( w->nodes().size() == 1 );
    CHECK_EQ( w->nodes()[0].children[1], -3 );
    CHECK( w->nodes()[0].mins.x == -128.0f );
    REQUIRE( w->marksurfaces().size() == 2 );
    CHECK_EQ( w->marksurfaces()[1], 1 );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_minimal_world_loads );
    RUN_TEST( test_plane_signbits );
    RUN_TEST( test_submodel_bounds_spread );
    RUN_TEST( test_leaf_clusters_world );
    RUN_TEST( test_leaf_clusters_bmodel );
    RUN_TEST( test_leaf0_not_solid_fails );
    RUN_TEST( test_nodes_loaded );
    RUN_TEST( test_node_child_validation );
    RUN_TEST( test_marksurface_fixup );
    RUN_TEST( test_marksurface_out_of_range_fails );
    RUN_TEST( test_missing_required_lump );
    RUN_TEST( test_malformed_worldspawn );
    RUN_TEST( test_bsp2_world_loads );

    std::printf( "bsp_lumps: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
