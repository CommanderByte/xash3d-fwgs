// xash3dpp — BSP header/version/quirk detection + lump resolution (Chunk 5, C3)
// Covers: version dispatch (29/30/'BSP2'/unknown/truncated), BSP30ext id
// probe incl. the id-only nuance, the extended-clipnode guess (both trigger
// conditions), Blue-Shift entities/planes swap detection + swapped
// resolution, and the Mod_LoadLump validation ladder (absent, misaligned,
// mincount, CHECK_OVERFLOW, overflow-warn, out-of-bounds hardening).
// Legacy reference: mod_bmodel.c:760-952, :4064-4068, :4242-4295.

#include <xash3dpp/private/map_loader/bsp/bsp_loader.hpp>

#include "test_bsp_builder.hpp"

#include "../../test_helpers.hpp"

#include <vector>

namespace bsp = xash::map_loader::bsp;
using test_bsp::TestBspBuilder;
using xash::core::ErrorCode;
using xash::map_loader::BspVersion;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// version dispatch
// ---------------------------------------------------------------------------

static void test_version_dispatch()
{
    {
        const auto file = TestBspBuilder{ bsp::k_hlbsp_version }.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        CHECK( hi->version == BspVersion::HalfLife );
        CHECK( !hi->bsp30ext );
        CHECK( !hi->blueshift_swap );
        CHECK( !hi->clipnodes32 );
    }
    {
        const auto file = TestBspBuilder{ bsp::k_q1bsp_version }.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        CHECK( hi->version == BspVersion::Quake1 );
        CHECK( !hi->clipnodes32 );
    }
    {
        const auto file = TestBspBuilder{ bsp::k_qbsp2_version }.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        CHECK( hi->version == BspVersion::Bsp2 );
        CHECK( hi->clipnodes32 ); // BSP2: 32-bit records everywhere
    }
    {
        const auto file = TestBspBuilder{ 31 }.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( !hi.has_value() );
        CHECK( hi.error() == ErrorCode::BspUnsupportedVersion );
    }
    {
        // Truncated header.
        auto file = TestBspBuilder{ bsp::k_hlbsp_version }.build();
        file.resize( sizeof( bsp::dheader_t ) - 4 );
        const auto hi = bsp::parse_header( file );
        REQUIRE( !hi.has_value() );
        CHECK( hi.error() == ErrorCode::BspCorruptLump );
    }
}

// ---------------------------------------------------------------------------
// BSP30ext probe + extended clipnode guess
// ---------------------------------------------------------------------------

static void test_bsp30ext_detection()
{
    // Clipnodes lump of 16 bytes: divisible by 8, 16/12 tiny → stays 16-bit.
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.enable_bsp30ext();
        const std::vector<bsp::dclipnode_t> nodes( 2 );
        b.set_lump_records( bsp::k_lump_clipnodes, nodes );
        const auto hi = bsp::parse_header( b.build() );
        REQUIRE( hi.has_value() );
        CHECK( hi->version == BspVersion::HalfLifeExt );
        CHECK( hi->bsp30ext );
        CHECK( !hi->clipnodes32 );
    }
    // Guess trigger 1: filelen % 8 != 0 (a single 12-byte record).
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.enable_bsp30ext();
        const std::vector<bsp::dclipnode32_t> nodes( 1 );
        b.set_lump_records( bsp::k_lump_clipnodes, nodes );
        const auto hi = bsp::parse_header( b.build() );
        REQUIRE( hi.has_value() );
        CHECK( hi->clipnodes32 );
    }
    // Guess trigger 2: divisible by 8 AND 12, but filelen/12 >= 32767.
    // lcm(8,12) = 24; 24 * 16384 = 393216; 393216/12 = 32768.
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.enable_bsp30ext();
        const std::vector<std::byte> big( 393216 );
        b.set_lump_bytes( bsp::k_lump_clipnodes, big.data(), big.size() );
        const auto hi = bsp::parse_header( b.build() );
        REQUIRE( hi.has_value() );
        CHECK( hi->clipnodes32 );
    }
    // Legacy nuance: the flag is gated on the extra-header ID ONLY — a
    // mismatched extra VERSION still counts as bsp30ext (mod_bmodel.c:4263
    // reads *extident; the version field gates extra-LUMP loading instead).
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.enable_bsp30ext( bsp::k_extra_header_id, /*version=*/3 );
        const auto hi = bsp::parse_header( b.build() );
        REQUIRE( hi.has_value() );
        CHECK( hi->bsp30ext );
        CHECK( hi->version == BspVersion::HalfLifeExt );
    }
    // Wrong id → plain HalfLife.
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.enable_bsp30ext( /*id=*/0x11223344, bsp::k_extra_version );
        const auto hi = bsp::parse_header( b.build() );
        REQUIRE( hi.has_value() );
        CHECK( !hi->bsp30ext );
        CHECK( hi->version == BspVersion::HalfLife );
    }
}

// ---------------------------------------------------------------------------
// Blue-Shift swap detection
// ---------------------------------------------------------------------------

static void test_blueshift_detection()
{
    // Swapped map: directory entry 0 points at plane-ish bytes, entry 1 at
    // the entity text (containing "classname").
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.set_entities( test_bsp::k_worldspawn_entities );
        const std::vector<bsp::dplane_t> planes( 2 );
        b.set_lump_records( bsp::k_lump_planes, planes );
        b.swap_entities_planes();

        const auto file = b.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        CHECK( hi->blueshift_swap );

        // resolve_lump(entities) must follow the swap and return the text.
        const auto ents = bsp::resolve_lump( file, *hi, bsp::k_lump_entities );
        REQUIRE( ents.has_value() && ents->present );
        CHECK_EQ( ents->bytes.size(), test_bsp::k_worldspawn_entities.size() );
        CHECK_EQ( static_cast<char>( ents->bytes[0] ), '{' );

        const auto pl = bsp::resolve_lump( file, *hi, bsp::k_lump_planes );
        REQUIRE( pl.has_value() && pl->present );
        CHECK_EQ( pl->count, std::size_t{ 2 } );
        CHECK_EQ( pl->entrysize, sizeof( bsp::dplane_t ));
    }
    // Normal map: entities contain "classname" → no swap.
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.set_entities( test_bsp::k_worldspawn_entities );
        const std::vector<bsp::dplane_t> planes( 2 );
        b.set_lump_records( bsp::k_lump_planes, planes );
        const auto hi = bsp::parse_header( b.build() );
        REQUIRE( hi.has_value() );
        CHECK( !hi->blueshift_swap );
    }
    // BSP30ext takes precedence: the swap probe is skipped entirely.
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.enable_bsp30ext();
        b.set_entities( "no-classname-here paddington padding" );
        b.set_lump_bytes( bsp::k_lump_planes,
                          test_bsp::k_worldspawn_entities.data(),
                          test_bsp::k_worldspawn_entities.size() );
        const auto hi = bsp::parse_header( b.build() );
        REQUIRE( hi.has_value() );
        CHECK( !hi->blueshift_swap );
    }
}

// ---------------------------------------------------------------------------
// lump validation ladder
// ---------------------------------------------------------------------------

static void test_resolve_validation()
{
    // Absent lump (fileofs 0) → silently not present, even for required lumps.
    {
        const auto file = TestBspBuilder{ bsp::k_hlbsp_version }.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        const auto lv = bsp::resolve_lump( file, *hi, bsp::k_lump_planes );
        REQUIRE( lv.has_value() );
        CHECK( !lv->present );
        CHECK_EQ( lv->count, std::size_t{ 0 } );
    }
    // Misaligned lump size (30 bytes is not a multiple of sizeof(dplane_t)).
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        const std::vector<std::byte> junk( 30 );
        b.set_lump_bytes( bsp::k_lump_planes, junk.data(), junk.size() );
        const auto file = b.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        const auto lv = bsp::resolve_lump( file, *hi, bsp::k_lump_planes );
        REQUIRE( !lv.has_value() );
        CHECK( lv.error() == ErrorCode::BspCorruptLump );
    }
    // Entities below the 32-byte mincount → error; at/above → ok.
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.set_entities( "too short" );
        const auto file = b.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        const auto lv = bsp::resolve_lump( file, *hi, bsp::k_lump_entities );
        REQUIRE( !lv.has_value() );
        CHECK( lv.error() == ErrorCode::BspCorruptLump );
    }
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        b.set_entities( test_bsp::k_worldspawn_entities );
        const auto file = b.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        const auto lv = bsp::resolve_lump( file, *hi, bsp::k_lump_entities );
        REQUIRE( lv.has_value() && lv->present );
        CHECK_EQ( lv->count, test_bsp::k_worldspawn_entities.size() );
    }
    // CHECK_OVERFLOW lump above maxcount → error (models: max 2048).
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        const std::vector<bsp::dmodel_t> models( bsp::k_max_map_models + 1 );
        b.set_lump_records( bsp::k_lump_models, models );
        const auto file = b.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        const auto lv = bsp::resolve_lump( file, *hi, bsp::k_lump_models );
        REQUIRE( !lv.has_value() );
        CHECK( lv.error() == ErrorCode::BspCorruptLump );
    }
    // Non-CHECK_OVERFLOW lump above maxcount → warn-and-proceed
    // (marksurfaces: max 524288, 2 bytes each).
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        const std::vector<bsp::dmarkface_t> marks( bsp::k_max_map_marksurfaces + 1 );
        b.set_lump_records( bsp::k_lump_marksurfaces, marks );
        const auto file = b.build();
        const auto hi   = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        const auto lv = bsp::resolve_lump( file, *hi, bsp::k_lump_marksurfaces );
        REQUIRE( lv.has_value() && lv->present );
        CHECK_EQ( lv->count, static_cast<std::size_t>( bsp::k_max_map_marksurfaces ) + 1 );
    }
    // Out-of-bounds lump (hardening; legacy has no such check): truncate the
    // file so the planes lump extends past the end.
    {
        TestBspBuilder b{ bsp::k_hlbsp_version };
        const std::vector<bsp::dplane_t> planes( 4 );
        b.set_lump_records( bsp::k_lump_planes, planes );
        auto file = b.build();
        file.resize( file.size() - 8 );
        const auto hi = bsp::parse_header( file );
        REQUIRE( hi.has_value() );
        const auto lv = bsp::resolve_lump( file, *hi, bsp::k_lump_planes );
        REQUIRE( !lv.has_value() );
        CHECK( lv.error() == ErrorCode::BspCorruptLump );
    }
}

// ---------------------------------------------------------------------------
// BSP2 record widths
// ---------------------------------------------------------------------------

static void test_bsp2_record_widths()
{
    TestBspBuilder b{ bsp::k_qbsp2_version };
    const std::vector<bsp::dnode32_t>     nodes( 3 );
    const std::vector<bsp::dleaf32_t>     leafs( 2 );
    const std::vector<bsp::dclipnode32_t> clips( 5 );
    const std::vector<bsp::dmarkface32_t> marks( 7 );
    b.set_lump_records( bsp::k_lump_nodes, nodes );
    b.set_lump_records( bsp::k_lump_leafs, leafs );
    b.set_lump_records( bsp::k_lump_clipnodes, clips );
    b.set_lump_records( bsp::k_lump_marksurfaces, marks );

    const auto file = b.build();
    const auto hi   = bsp::parse_header( file );
    REQUIRE( hi.has_value() );
    CHECK( hi->version == BspVersion::Bsp2 );

    const auto n = bsp::resolve_lump( file, *hi, bsp::k_lump_nodes );
    REQUIRE( n.has_value() && n->present );
    CHECK_EQ( n->entrysize, sizeof( bsp::dnode32_t ));
    CHECK_EQ( n->count, std::size_t{ 3 } );

    const auto l = bsp::resolve_lump( file, *hi, bsp::k_lump_leafs );
    REQUIRE( l.has_value() && l->present );
    CHECK_EQ( l->entrysize, sizeof( bsp::dleaf32_t ));
    CHECK_EQ( l->count, std::size_t{ 2 } );

    const auto c = bsp::resolve_lump( file, *hi, bsp::k_lump_clipnodes );
    REQUIRE( c.has_value() && c->present );
    CHECK_EQ( c->entrysize, sizeof( bsp::dclipnode32_t ));
    CHECK_EQ( c->count, std::size_t{ 5 } );

    const auto m = bsp::resolve_lump( file, *hi, bsp::k_lump_marksurfaces );
    REQUIRE( m.has_value() && m->present );
    CHECK_EQ( m->entrysize, sizeof( bsp::dmarkface32_t ));
    CHECK_EQ( m->count, std::size_t{ 7 } );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_version_dispatch );
    RUN_TEST( test_bsp30ext_detection );
    RUN_TEST( test_blueshift_detection );
    RUN_TEST( test_resolve_validation );
    RUN_TEST( test_bsp2_record_widths );

    std::printf( "bsp_header: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
