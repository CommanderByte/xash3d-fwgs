// xash3dpp — clipnode widening + hull construction (Chunk 5, C5)
// Covers: 16→32 widening incl. the aguirRe broken-clipnode wrap, hull-0
// construction from drawing nodes, per-submodel hull wiring (hull-bounds
// remap, ZHLT empty hulls, optimizer -1 headnodes, injectable bounds
// table), BSP30ext per-hull remap (incl. its stricter missed-hull rules),
// and "*N" origin detection + MODEL_HAS_ORIGIN.
// Legacy reference: mod_bmodel.c Mod_LoadClipnodes / Mod_MakeHull0 /
// Mod_SetupHull / Mod_SetupSubmodels / Mod_FindModelOrigin.

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
using test_bsp::TestBspBuilder;
using xash::core::ErrorCode;

static int g_pass = 0, g_fail = 0;

static ml::WorldLoadOptions world_opts() { return ml::WorldLoadOptions{}; }

// ---------------------------------------------------------------------------
// widening
// ---------------------------------------------------------------------------

static void test_clipnodes_widened()
{
    const auto w = ml::load_world_data( make_minimal_world().build(), "t", world_opts() );
    REQUIRE( w.has_value() );

    const auto clips = w->clipnodes();
    REQUIRE( clips.size() == 2 );
    CHECK_EQ( clips[0].planenum, 0 );
    CHECK_EQ( clips[0].children[0], -1 ); // CONTENTS_EMPTY through the u16 wrap
    CHECK_EQ( clips[0].children[1], 1 );
    CHECK_EQ( clips[1].planenum, 1 );
    CHECK_EQ( clips[1].children[0], -2 ); // CONTENTS_SOLID
}

static void test_aguirre_wrap()
{
    // Child index 3 with only 2 clipnodes: (u16)3 >= 2 → 3 - 65536 = -65533.
    auto b = make_minimal_world();
    const std::vector<bsp::dclipnode_t> clips = {
        { 0, { 3, 1 } },
        { 1, { -2, -1 } },
    };
    b.set_lump_records( bsp::k_lump_clipnodes, clips );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    CHECK_EQ( w->clipnodes()[0].children[0], 3 - 65536 );
    CHECK_EQ( w->clipnodes()[0].children[1], 1 );
}

// ---------------------------------------------------------------------------
// hull 0
// ---------------------------------------------------------------------------

static void test_hull0_from_nodes()
{
    const auto w = ml::load_world_data( make_minimal_world().build(), "t", world_opts() );
    REQUIRE( w.has_value() );

    const auto h0 = w->hull0_nodes();
    REQUIRE( h0.size() == 1 );
    CHECK_EQ( h0[0].planenum, 0 );
    CHECK_EQ( h0[0].children[0], ml::k_contents_empty ); // leaf 1 contents
    CHECK_EQ( h0[0].children[1], ml::k_contents_water ); // leaf 2 contents

    const auto &d0 = w->submodels()[0].hulls[0];
    CHECK( d0.present );
    CHECK_EQ( d0.firstclipnode, 0 );
    CHECK_EQ( d0.lastclipnode, 1 ); // headnode 0 + 1 counted node (legacy off-by-one kept)
}

// ---------------------------------------------------------------------------
// hulls 1-3, non-ext
// ---------------------------------------------------------------------------

static void test_hull_wiring()
{
    const auto w = ml::load_world_data( make_minimal_world().build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    const auto &m = w->submodels()[0];

    // hull 1 ← usehull 0 (human).
    const auto &h1 = m.hulls[1];
    CHECK( h1.present );
    CHECK_EQ( h1.firstclipnode, 0 );
    CHECK_EQ( h1.lastclipnode, 1 ); // numclipnodes - 1
    CHECK( h1.clip_mins.z == -36.0f && h1.clip_maxs.z == 36.0f );

    // hull 2 ← usehull 3 (large); headnode -1 passes the ZHLT check and is
    // kept raw (kernel yields immediate CONTENTS_EMPTY — legacy optimizer
    // quirk, mod_bmodel.c:2012-2016).
    const auto &h2 = m.hulls[2];
    CHECK( h2.present );
    CHECK_EQ( h2.firstclipnode, -1 );
    CHECK_EQ( h2.lastclipnode, 1 );
    CHECK( h2.clip_mins.x == -32.0f );

    // hull 3 ← usehull 1 (head/duck).
    const auto &h3 = m.hulls[3];
    CHECK( h3.present );
    CHECK( h3.clip_mins.z == -18.0f && h3.clip_maxs.z == 18.0f );
}

static void test_zhlt_empty_hull()
{
    auto b = make_minimal_world();
    std::vector<bsp::dmodel_t> models( 1 );
    models[0] = { { -64, -64, -64 }, { 64, 64, 64 }, {},
                  { 0, 5, -1, -1 }, // hull1 headnode 5 >= numclipnodes (2)
                  2, 0, 2 };
    b.set_lump_records( bsp::k_lump_models, models );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    const auto &h1 = w->submodels()[0].hulls[1];
    CHECK( !h1.present );
    CHECK_EQ( h1.firstclipnode, 0 );
    CHECK_EQ( h1.lastclipnode, 0 );
}

static void test_injectable_hull_bounds()
{
    // Nulling the usehull-0 slot makes BSP hull 1 "no hull specified".
    ml::WorldLoadOptions opts;
    opts.hull_bounds[0] = { { 0, 0, 0 }, { 0, 0, 0 } };

    const auto w = ml::load_world_data( make_minimal_world().build(), "t", opts );
    REQUIRE( w.has_value() );
    CHECK( !w->submodels()[0].hulls[1].present );

    // Custom (mod-provided) extents flow through to the descriptor.
    ml::WorldLoadOptions custom;
    custom.hull_bounds[0] = { { -12, -12, -24 }, { 12, 12, 24 } };
    const auto w2 = ml::load_world_data( make_minimal_world().build(), "t", custom );
    REQUIRE( w2.has_value() );
    CHECK( w2->submodels()[0].hulls[1].clip_maxs.z == 24.0f );
}

// ---------------------------------------------------------------------------
// BSP30ext remap
// ---------------------------------------------------------------------------

static void test_bsp30ext_remap()
{
    auto b = make_minimal_world();
    b.enable_bsp30ext();

    // hull1 rooted at clipnode 1 → remapped array = just node 1, compacted.
    std::vector<bsp::dmodel_t> models( 1 );
    models[0] = { { -64, -64, -64 }, { 64, 64, 64 }, {},
                  { 0, 1, -1, -1 }, 2, 0, 2 };
    b.set_lump_records( bsp::k_lump_models, models );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );

    // For ext maps clipnodes() holds ONLY the remapped per-hull arrays.
    const auto clips = w->clipnodes();
    REQUIRE( clips.size() == 1 );
    CHECK_EQ( clips[0].planenum, 1 );
    CHECK_EQ( clips[0].children[0], -2 );
    CHECK_EQ( clips[0].children[1], -1 );

    const auto &h1 = w->submodels()[0].hulls[1];
    CHECK( h1.present );
    CHECK_EQ( h1.firstclipnode, 0 );
    CHECK_EQ( h1.lastclipnode, 1 ); // emitted count (legacy semantics kept)

    // Ext path treats -1 headnodes as MISSED (stricter than non-ext).
    CHECK( !w->submodels()[0].hulls[2].present );
    CHECK( !w->submodels()[0].hulls[3].present );
}

static void test_bsp30ext_remap_full_tree()
{
    auto b = make_minimal_world();
    b.enable_bsp30ext();
    // hull1 rooted at clipnode 0 → preorder re-emission of both nodes.
    // (fixture dmodel already uses headnode[1] = 0)

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );

    const auto clips = w->clipnodes();
    REQUIRE( clips.size() == 2 );
    CHECK_EQ( clips[0].planenum, 0 );
    CHECK_EQ( clips[0].children[0], -1 );
    CHECK_EQ( clips[0].children[1], 1 ); // remapped index of node 1
    CHECK_EQ( clips[1].planenum, 1 );
    CHECK_EQ( clips[1].children[0], -2 );

    const auto &h1 = w->submodels()[0].hulls[1];
    CHECK_EQ( h1.firstclipnode, 0 );
    CHECK_EQ( h1.lastclipnode, 2 );
}

// ---------------------------------------------------------------------------
// "*N" origin detection
// ---------------------------------------------------------------------------

static void test_model_origin_detection()
{
    auto b = make_minimal_world();

    const std::string ents =
        std::string( test_bsp::k_minimal_world_entities ) +
        "{\n\"classname\" \"func_door\"\n\"model\" \"*1\"\n\"origin\" \"16 32 -8\"\n}\n"
        "{\n\"classname\" \"func_wall\"\n\"model\" \"*2\"\n}\n";
    b.set_entities( ents );

    std::vector<bsp::dmodel_t> models( 3 );
    models[0] = { { -64, -64, -64 }, { 64, 64, 64 }, {}, { 0, 0, -1, -1 }, 2, 0, 2 };
    models[1] = { { -8, -8, -8 }, { 8, 8, 8 }, {}, { 0, 0, -1, -1 }, 0, 0, 0 };
    models[2] = { { -8, -8, -8 }, { 8, 8, 8 }, {}, { 0, 0, -1, -1 }, 0, 0, 0 };
    b.set_lump_records( bsp::k_lump_models, models );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    REQUIRE( w->submodels().size() == 3 );

    const auto &door = w->submodels()[1];
    CHECK( door.origin.x == 16.0f && door.origin.y == 32.0f && door.origin.z == -8.0f );
    CHECK( ( door.flags & ml::k_model_has_origin ) != 0 );

    const auto &wall = w->submodels()[2];
    CHECK( wall.origin.x == 0.0f );
    CHECK_EQ( wall.flags & ml::k_model_has_origin, 0u );

    // The world model never gets the origin scan.
    CHECK_EQ( w->submodels()[0].flags & ml::k_model_has_origin, 0u );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_clipnodes_widened );
    RUN_TEST( test_aguirre_wrap );
    RUN_TEST( test_hull0_from_nodes );
    RUN_TEST( test_hull_wiring );
    RUN_TEST( test_zhlt_empty_hull );
    RUN_TEST( test_injectable_hull_bounds );
    RUN_TEST( test_bsp30ext_remap );
    RUN_TEST( test_bsp30ext_remap_full_tree );
    RUN_TEST( test_model_origin_detection );

    std::printf( "bsp_hulls: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
