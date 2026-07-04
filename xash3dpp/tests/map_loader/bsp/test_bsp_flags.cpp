// xash3dpp — texture names, surface flags, model flags, water-alpha (C6)
// Covers: miptex name extraction (lowercase, "*default" for missing data,
// "miptex_N" for empty names), texinfo miptex clamp, every name/flag rule
// from the Mod_LoadSurfaces flag block, the corrupt-face guard, submodel
// MODEL_* derivation (i != 0 only), and the water-alpha probe.
// Legacy reference: mod_bmodel.c Mod_LoadTexture/Mod_LoadTexInfo/
// Mod_LoadSurfaces/Mod_LooksLikeWaterTexture/Mod_CheckWaterAlphaSupport.

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
using test_bsp::make_textures_lump;

static int g_pass = 0, g_fail = 0;

static ml::WorldLoadOptions world_opts() { return ml::WorldLoadOptions{}; }

// Builds the minimal world with a custom texture/texinfo/face set: one face
// per texture, each face's texinfo i referencing miptex i.
static test_bsp::TestBspBuilder
world_with_textures( const std::vector<std::string> &names,
                     std::int16_t texflags = 0 )
{
    auto b = make_minimal_world();

    const auto tex = make_textures_lump( names );
    b.set_lump_bytes( bsp::k_lump_textures, tex.data(), tex.size() );

    std::vector<bsp::dtexinfo_t> ti( names.size() );
    for ( std::size_t i = 0; i < names.size(); ++i )
    {
        ti[i] = {};
        ti[i].miptex = static_cast<std::int32_t>( i );
        ti[i].flags  = texflags;
    }
    b.set_lump_records( bsp::k_lump_texinfo, ti );

    std::vector<bsp::dface_t> faces( names.size() );
    for ( std::size_t i = 0; i < names.size(); ++i )
    {
        faces[i] = {};
        faces[i].texinfo = static_cast<std::int16_t>( i );
    }
    b.set_lump_records( bsp::k_lump_faces, faces );

    // marksurfaces must stay within the face count.
    const std::vector<bsp::dmarkface_t> marks = { 0 };
    b.set_lump_records( bsp::k_lump_marksurfaces, marks );

    return b;
}

// ---------------------------------------------------------------------------
// texture names
// ---------------------------------------------------------------------------

static void test_texture_name_extraction()
{
    const auto w = ml::load_world_data(
        world_with_textures( { "WALL_A", "\x01missing", "" } ).build(), "t", world_opts() );
    REQUIRE( w.has_value() );

    const auto names = w->texture_names();
    REQUIRE( names.size() == 3 );
    CHECK( names[0] == "wall_a" );    // lowercased
    CHECK( names[1] == "*default" );  // dataofs -1 → default texture
    CHECK( names[2] == "miptex_2" );  // empty name fallback
}

static void test_texinfo_miptex_clamp()
{
    auto b = make_minimal_world();
    std::vector<bsp::dtexinfo_t> ti( 1 );
    ti[0] = {};
    ti[0].miptex = 42; // out of range (1 texture) → clamped to 0
    b.set_lump_records( bsp::k_lump_texinfo, ti );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    CHECK_EQ( w->texinfos()[0].miptex, 0 );
}

// ---------------------------------------------------------------------------
// surface flags
// ---------------------------------------------------------------------------

static void test_surface_flag_rules()
{
    const auto w = ml::load_world_data(
        world_with_textures( { "wall", "sky_day", "!water0", "*lava1", "water4b",
                               "laser_beam", "{fence", "scrollmove", "{scrollconv",
                               "*default" } ).build(),
        "t", world_opts() );
    REQUIRE( w.has_value() );

    const auto s = w->surfaces();
    REQUIRE( s.size() == 10 );
    CHECK_EQ( s[0].flags, 0u );                                        // plain
    CHECK_EQ( s[1].flags, ml::k_surf_drawsky );                        // sky*
    CHECK_EQ( s[2].flags, ml::k_surf_drawturb );                       // !*
    CHECK_EQ( s[3].flags, ml::k_surf_drawturb );                       // ** (lava name still turb)
    CHECK_EQ( s[4].flags, ml::k_surf_drawturb );                       // water*
    CHECK_EQ( s[5].flags, ml::k_surf_drawturb );                       // laser*
    CHECK_EQ( s[6].flags, ml::k_surf_transparent );                    // {…
    CHECK_EQ( s[7].flags, ml::k_surf_conveyor );                       // scroll*
    CHECK_EQ( s[8].flags, ml::k_surf_conveyor | ml::k_surf_transparent ); // {scroll*
    CHECK_EQ( s[9].flags, 0u );                                        // *default is NOT water
}

static void test_texinfo_flag_rules()
{
    {
        const auto w = ml::load_world_data(
            world_with_textures( { "wall" }, bsp::k_tex_scroll ).build(), "t", world_opts() );
        REQUIRE( w.has_value() );
        CHECK_EQ( w->surfaces()[0].flags, ml::k_surf_conveyor ); // TEX_SCROLL
    }
    {
        const auto w = ml::load_world_data(
            world_with_textures( { "wall" }, bsp::k_tex_special ).build(), "t", world_opts() );
        REQUIRE( w.has_value() );
        CHECK_EQ( w->surfaces()[0].flags, ml::k_surf_drawtiled ); // TEX_SPECIAL
    }
}

static void test_corrupt_face_guard()
{
    auto b = world_with_textures( { "!water0" } );
    std::vector<bsp::dface_t> faces( 1 );
    faces[0] = {};
    faces[0].texinfo   = 0;
    faces[0].firstedge = 6;
    faces[0].numedges  = 4; // 6 + 4 > 8 surfedges → guard trips
    b.set_lump_records( bsp::k_lump_faces, faces );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    CHECK_EQ( w->surfaces()[0].flags, 0u ); // no flags derived
}

// ---------------------------------------------------------------------------
// submodel MODEL_* flags
// ---------------------------------------------------------------------------

static void test_submodel_flags_from_surfaces()
{
    // Faces: 0 = water (LIQUID), 1 = {fence (TRANSPARENT), 2 = scroll
    // (CONVEYOR).  Submodel 1 spans faces 0-1, submodel 2 spans face 2,
    // world spans everything (and must stay flagless — the legacy loop is
    // gated on i != 0).
    auto b = world_with_textures( { "!water0", "{fence", "scrollmove" } );

    std::vector<bsp::dmodel_t> models( 3 );
    models[0] = { { -64, -64, -64 }, { 64, 64, 64 }, {}, { 0, 0, -1, -1 }, 2, 0, 3 };
    models[1] = { { -8, -8, -8 }, { 8, 8, 8 }, {}, { 0, 0, -1, -1 }, 0, 0, 2 };
    models[2] = { { -8, -8, -8 }, { 8, 8, 8 }, {}, { 0, 0, -1, -1 }, 0, 2, 1 };
    b.set_lump_records( bsp::k_lump_models, models );

    const auto w = ml::load_world_data( b.build(), "t", world_opts() );
    REQUIRE( w.has_value() );
    REQUIRE( w->submodels().size() == 3 );

    CHECK_EQ( w->submodels()[0].flags & ( ml::k_model_liquid | ml::k_model_transparent |
                                          ml::k_model_conveyor ), 0u );
    CHECK( ( w->submodels()[1].flags & ml::k_model_liquid ) != 0 );
    CHECK( ( w->submodels()[1].flags & ml::k_model_transparent ) != 0 );
    CHECK_EQ( w->submodels()[1].flags & ml::k_model_conveyor, 0u );
    CHECK( ( w->submodels()[2].flags & ml::k_model_conveyor ) != 0 );
}

// ---------------------------------------------------------------------------
// water-alpha probe
// ---------------------------------------------------------------------------

static void test_wateralpha_probe()
{
    // Fixture: water leaf 2 (cluster 1) has visofs -1 → decompresses as
    // all-visible → sees the empty leaf 1 → supported.
    {
        const auto w = ml::load_world_data( make_minimal_world().build(), "t", world_opts() );
        REQUIRE( w.has_value() );
        CHECK( ( w->flags() & ml::k_fworld_wateralpha ) != 0 );
    }
    // No visdata at all → legacy returns true.
    {
        auto b = make_minimal_world();
        b.set_lump_bytes( bsp::k_lump_visibility, nullptr, 0 );
        // zero-length lump with nonzero fileofs → present=false … emulate
        // absent entirely by rebuilding without the lump is overkill; a
        // 0-length set_lump gives filelen 0 → absent → empty visdata.
        const auto w = ml::load_world_data( b.build(), "t", world_opts() );
        REQUIRE( w.has_value() );
        CHECK( ( w->flags() & ml::k_fworld_wateralpha ) != 0 );
    }
    // Liquid leaf that can only see solid/water → NOT supported.
    {
        auto b = make_minimal_world();
        std::vector<bsp::dleaf_t> leafs( 3 );
        leafs[0] = { -2, -1, {}, {}, 0, 0, {} };            // solid
        leafs[1] = { -3,  0, {}, {}, 0, 0, {} };            // water, cluster 0
        leafs[2] = { -3, -1, {}, {}, 0, 0, {} };            // water, cluster 1
        b.set_lump_records( bsp::k_lump_leafs, leafs );
        // vis row for cluster 0: only bit 1 set (sees cluster 1 = water).
        const unsigned char vis[2] = { 0x02, 0x00 };
        b.set_lump_bytes( bsp::k_lump_visibility, vis, sizeof vis );

        const auto w = ml::load_world_data( b.build(), "t", world_opts() );
        REQUIRE( w.has_value() );
        CHECK_EQ( w->flags() & ml::k_fworld_wateralpha, 0u );
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_texture_name_extraction );
    RUN_TEST( test_texinfo_miptex_clamp );
    RUN_TEST( test_surface_flag_rules );
    RUN_TEST( test_texinfo_flag_rules );
    RUN_TEST( test_corrupt_face_guard );
    RUN_TEST( test_submodel_flags_from_surfaces );
    RUN_TEST( test_wateralpha_probe );

    std::printf( "bsp_flags: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
