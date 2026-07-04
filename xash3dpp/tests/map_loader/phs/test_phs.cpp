// xash3dpp — PHS build + query tests (Chunk 6 S3, Q-19)
// Covers: compress_pvs goldens (zero-RLE, 255-run cap) + round-trip,
// build_phs on a hand-derived 3-cluster chain world where PHS != PVS
// (cluster 0 sees {0,1} but hears {0,1,2}), the fat_phs radius walk and
// its fullvis/empty-table/clusterless fallbacks, headnode_visible
// traversal order, and an independent naive-fold cross-check of the
// whole table (hand-derived + reference-reimplementation convention).
//
// Chain fixture (make_chain_world): planes x=128 (n0), x=-128 (n1);
// leafs: 0 solid / 1 (x>128, cluster 0, visofs 0) / 2 (-128<x<128,
// cluster 1, visofs 1) / 3 (x<-128, cluster 2, visofs 2);
// visdata = {0x03, 0x07, 0x06}, visleafs 3 → visbytes 1.
// Hand-derived PHS rows (rowbytes 4): row0 FF, rows 1-3 all 0x07 —
// each compresses to {row, 0x00, 0x03}, offsets {0,3,6,9}.

#include <xash3dpp/map_loader/phs.hpp>
#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/map_loader/world.hpp>

#include "../bsp/test_bsp_builder.hpp"

#include "../../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ml = xash::map_loader;
namespace bsp = ml::bsp;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// fixtures
// ---------------------------------------------------------------------------

// solid_back routes node 1's back child to the solid leaf 0 (cluster -1)
// so a reachable clusterless leaf exists for the fallback tests.
static test_bsp::TestBspBuilder make_chain_world( bool with_vis = true,
                                                  bool solid_back = false )
{
    test_bsp::TestBspBuilder b{ bsp::k_hlbsp_version };

    b.set_entities( test_bsp::k_minimal_world_entities );

    const std::vector<bsp::dplane_t> planes = {
        { { 1.0f, 0.0f, 0.0f },  128.0f, bsp::k_plane_x },
        { { 1.0f, 0.0f, 0.0f }, -128.0f, bsp::k_plane_x },
    };
    b.set_lump_records( bsp::k_lump_planes, planes );

    const std::vector<bsp::dface_t> faces( 2 );
    b.set_lump_records( bsp::k_lump_faces, faces );
    const std::vector<bsp::dmarkface_t> marks = { 0, 1 };
    b.set_lump_records( bsp::k_lump_marksurfaces, marks );

    std::vector<bsp::dleaf_t> leafs( 4 );
    leafs[0] = { -2, -1, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, { 1, 2, 3, 4 } };
    leafs[1] = { -1,  0, { 128, -256, -256 }, { 256, 256, 256 }, 0, 2, {} };
    leafs[2] = { -1,  1, { -128, -256, -256 }, { 128, 256, 256 }, 0, 0, {} };
    leafs[3] = { -1,  2, { -256, -256, -256 }, { -128, 256, 256 }, 0, 0, {} };
    b.set_lump_records( bsp::k_lump_leafs, leafs );

    std::vector<bsp::dnode_t> nodes( 2 );
    nodes[0] = { 0, { -2, 1 },  { -256, -256, -256 }, { 256, 256, 256 }, 0, 2 };
    nodes[1] = { 1, { -3, static_cast<std::int16_t>( solid_back ? -1 : -4 ) },
                 { -256, -256, -256 }, { 128, 256, 256 }, 0, 0 };
    b.set_lump_records( bsp::k_lump_nodes, nodes );

    const std::vector<bsp::dclipnode_t> clips = {
        { 0, { -1, 1 } },
        { 1, { -2, -1 } },
    };
    b.set_lump_records( bsp::k_lump_clipnodes, clips );

    std::vector<bsp::dmodel_t> models( 1 );
    models[0] = { { -256.0f, -256.0f, -256.0f },
                  {  256.0f,  256.0f,  256.0f },
                  {    0.0f,    0.0f,    0.0f },
                  { 0, 0, -1, -1 },
                  /*visleafs=*/3, /*firstface=*/0, /*numfaces=*/2 };
    b.set_lump_records( bsp::k_lump_models, models );

    // Per-cluster compressed PVS rows (single nonzero byte each):
    // cluster 0 sees {0,1}; cluster 1 sees {0,1,2}; cluster 2 sees {1,2}.
    if ( with_vis )
    {
        const unsigned char vis[3] = { 0x03, 0x07, 0x06 };
        b.set_lump_bytes( bsp::k_lump_visibility, vis, sizeof vis );
    }

    const auto tex = test_bsp::make_textures_lump( { "wall" } );
    b.set_lump_bytes( bsp::k_lump_textures, tex.data(), tex.size() );
    const std::vector<bsp::dtexinfo_t> ti( 1 );
    b.set_lump_records( bsp::k_lump_texinfo, ti );
    const std::vector<bsp::dsurfedge_t> surfedges( 8 );
    b.set_lump_records( bsp::k_lump_surfedges, surfedges );

    return b;
}

static ml::WorldData load_world( const test_bsp::TestBspBuilder &b,
                                 bool is_world = true )
{
    ml::WorldLoadOptions opts;
    opts.is_world = is_world;
    auto w = ml::load_world_data( b.build(), "t", opts );
    REQUIRE( w.has_value() );
    return std::move( *w );
}

static unsigned first_byte( std::span<const std::byte> s )
{
    REQUIRE( !s.empty() );
    return static_cast<unsigned char>( s[0] );
}

// ---------------------------------------------------------------------------
// compress_pvs
// ---------------------------------------------------------------------------

static void test_compress_goldens()
{
    std::array<std::byte, 16> out{};

    const std::array<std::byte, 4> row07 = { std::byte{ 0x07 }, std::byte{ 0 },
                                             std::byte{ 0 }, std::byte{ 0 } };
    REQUIRE( ml::compress_pvs( row07, out ) == 3 );
    CHECK_EQ( static_cast<unsigned>( out[0] ), 0x07u );
    CHECK_EQ( static_cast<unsigned>( out[1] ), 0x00u );
    CHECK_EQ( static_cast<unsigned>( out[2] ), 0x03u );

    const std::array<std::byte, 4> zeros{};
    REQUIRE( ml::compress_pvs( zeros, out ) == 2 );
    CHECK_EQ( static_cast<unsigned>( out[0] ), 0x00u );
    CHECK_EQ( static_cast<unsigned>( out[1] ), 0x04u );

    // Nonzero bytes copy through uncompressed.
    const std::array<std::byte, 2> raw = { std::byte{ 0xAA }, std::byte{ 0xBB } };
    REQUIRE( ml::compress_pvs( raw, out ) == 2 );
    CHECK_EQ( static_cast<unsigned>( out[0] ), 0xAAu );
    CHECK_EQ( static_cast<unsigned>( out[1] ), 0xBBu );
}

static void test_compress_run_cap()
{
    // A 300-zero run splits at the 255 cap: {00 FF 00 2D}.
    std::vector<std::byte> in( 300, std::byte{ 0 } );
    std::vector<std::byte> out( 600 );
    REQUIRE( ml::compress_pvs( in, out ) == 4 );
    CHECK_EQ( static_cast<unsigned>( out[0] ), 0x00u );
    CHECK_EQ( static_cast<unsigned>( out[1] ), 0xFFu );
    CHECK_EQ( static_cast<unsigned>( out[2] ), 0x00u );
    CHECK_EQ( static_cast<unsigned>( out[3] ), 0x2Du );
}

static void test_compress_decompress_roundtrip()
{
    // Deterministic LCG-driven pattern with zero runs of varied length.
    std::vector<std::byte> in( 512 );
    std::uint32_t r = 0x12345678u;
    for ( auto &b : in )
    {
        r = r * 1664525u + 1013904223u;
        const auto v = static_cast<unsigned char>( r >> 24 );
        b = ( v & 0x03u ) ? std::byte{ v } : std::byte{ 0 };
    }

    std::vector<std::byte> compressed( in.size() * 2 );
    const std::size_t size = ml::compress_pvs( in, compressed );
    REQUIRE( size > 0 );

    std::vector<std::byte> back( in.size() );
    ml::decompress_pvs( std::span<const std::byte>( compressed ).first( size ),
                        in.size(), back );
    CHECK( back == in );
}

// ---------------------------------------------------------------------------
// build_phs — hand-derived goldens
// ---------------------------------------------------------------------------

static void test_build_phs_goldens()
{
    const auto w = load_world( make_chain_world() );
    REQUIRE( w.visbytes() == 1 );

    const ml::PhsTable phs = ml::build_phs( w );
    REQUIRE( !phs.empty() );
    REQUIRE( phs.row_count() == 4 );

    // Row 0 (solid leaf, visofs -1 → all-visible PVS): PHS row 0xFF.
    // Rows 1-3: hand-derived fold — every cluster ends up hearing {0,1,2}.
    std::array<std::byte, 1> row{};
    ml::decompress_pvs( phs.compressed_row( 0 ), 1, row );
    CHECK_EQ( static_cast<unsigned>( row[0] ), 0xFFu );

    for ( std::size_t i = 1; i < 4; ++i )
    {
        ml::decompress_pvs( phs.compressed_row( i ), 1, row );
        CHECK_EQ( static_cast<unsigned>( row[0] ), 0x07u );
    }

    // Compressed shape: each 4-byte row {v,0,0,0} → {v, 00, 03}.
    CHECK_EQ( phs.compressed_row( 1 ).size() -
              phs.compressed_row( 2 ).size(), 3u ); // offsets 3 apart
    CHECK_EQ( first_byte( phs.compressed_row( 0 )), 0xFFu );
    CHECK_EQ( first_byte( phs.compressed_row( 1 )), 0x07u );

    // Out-of-range row (hardening): empty span == all-visible.
    CHECK( phs.compressed_row( 99 ).empty() );
}

static void test_build_phs_no_visdata()
{
    // A world without a vis lump (novis map) → legacy early-return,
    // empty table — and fat_phs then falls back to full visibility.
    const auto w = load_world( make_chain_world( /*with_vis=*/false ));
    REQUIRE( w.visdata().empty() );
    CHECK( ml::build_phs( w ).empty() );
}

// ---------------------------------------------------------------------------
// naive reference cross-check (independent fold implementation)
// ---------------------------------------------------------------------------

static void test_build_phs_matches_naive_fold()
{
    const auto w = load_world( make_chain_world() );
    const ml::PhsTable phs = ml::build_phs( w );

    const std::size_t count = w.leafs().size();
    const std::size_t vb    = w.visbytes();

    // Reference: decompress all PVS rows, then fold by iterating CLUSTER
    // bits (a deliberately different loop shape from the byte/bit scan in
    // build_phs).
    std::vector<std::vector<std::byte>> pvs( count );
    for ( std::size_t i = 0; i < count; ++i )
    {
        pvs[i].resize( vb );
        ml::decompress_pvs( ml::leaf_compressed_pvs( w, static_cast<int>( i )),
                            vb, pvs[i] );
    }

    for ( std::size_t i = 0; i < count; ++i )
    {
        std::vector<std::byte> want = pvs[i];
        for ( std::size_t c = 0; c + 1 < count; ++c )
        {
            if ( !ml::check_vis_bit( pvs[i], static_cast<int>( c )))
                continue;
            for ( std::size_t b = 0; b < vb; ++b )
                want[b] = static_cast<std::byte>(
                    static_cast<unsigned char>( want[b] ) |
                    static_cast<unsigned char>( pvs[c + 1][b] ));
        }

        std::vector<std::byte> got( vb );
        ml::decompress_pvs( phs.compressed_row( i ), vb, got );
        CHECK( got == want );
    }
}

// ---------------------------------------------------------------------------
// fat_phs
// ---------------------------------------------------------------------------

static void test_fat_phs_vs_fat_pvs()
{
    const auto w = load_world( make_chain_world() );
    const ml::PhsTable phs = ml::build_phs( w );
    std::array<std::byte, 1> vis{};

    // org deep in cluster 0: PVS says {0,1}, PHS says {0,1,2}.
    const Vec3 org{ 200.0f, 0.0f, 0.0f };
    REQUIRE( ml::fat_pvs( w, org, 8.0f, vis, false, false ) == 1 );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0x03u );

    REQUIRE( ml::fat_phs( w, phs, org, 8.0f, vis, false, false ) == 1 );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0x07u );
}

static void test_fat_phs_radius_straddle_and_merge()
{
    const auto w = load_world( make_chain_world() );
    const ml::PhsTable phs = ml::build_phs( w );
    std::array<std::byte, 1> vis{};

    // Within 8 units of the x=128 plane → both sides accumulate.
    REQUIRE( ml::fat_phs( w, phs, { 130.0f, 0.0f, 0.0f }, 8.0f, vis,
                          false, false ) == 1 );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0x07u );

    // merge=true accumulates into existing contents instead of clearing.
    vis[0] = std::byte{ 0x80 };
    REQUIRE( ml::fat_phs( w, phs, { 200.0f, 0.0f, 0.0f }, 8.0f, vis,
                          true, false ) == 1 );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0x87u );
}

static void test_fat_phs_fallbacks()
{
    const auto w = load_world( make_chain_world() );
    const ml::PhsTable phs = ml::build_phs( w );
    std::array<std::byte, 1> vis{};

    // fullvis flag.
    REQUIRE( ml::fat_phs( w, phs, { 200.0f, 0.0f, 0.0f }, 8.0f, vis,
                          false, true ) == 1 );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0xFFu );

    // "requested PHS but we don't have PHS" — empty table → fullvis.
    vis[0] = std::byte{ 0 };
    const ml::PhsTable none;
    REQUIRE( ml::fat_phs( w, none, { 200.0f, 0.0f, 0.0f }, 8.0f, vis,
                          false, false ) == 1 );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0xFFu );

    // Point in a clusterless leaf (solid leaf 0 wired reachable) → fullvis.
    const auto sw = load_world(
        make_chain_world( /*with_vis=*/true, /*solid_back=*/true ));
    const ml::PhsTable sphs = ml::build_phs( sw );
    vis[0] = std::byte{ 0 };
    REQUIRE( ml::fat_phs( sw, sphs, { -200.0f, 0.0f, 0.0f }, 8.0f, vis,
                          false, false ) == 1 );
    CHECK_EQ( static_cast<unsigned>( vis[0] ), 0xFFu );
}

// ---------------------------------------------------------------------------
// headnode_visible
// ---------------------------------------------------------------------------

static void test_headnode_visible()
{
    const auto w = load_world( make_chain_world() );
    int lastleaf = -99;

    // Only cluster 2 set: traversal passes leafs 1 (cluster 0) and 2
    // (cluster 1) before hitting leaf 3.
    const std::array<std::byte, 1> only2 = { std::byte{ 0x04 } };
    REQUIRE( ml::headnode_visible( w, 0, only2, &lastleaf ));
    CHECK_EQ( lastleaf, 2 );

    // First visible leaf in front-first order wins.
    const std::array<std::byte, 1> all = { std::byte{ 0x07 } };
    REQUIRE( ml::headnode_visible( w, 0, all, &lastleaf ));
    CHECK_EQ( lastleaf, 0 );

    // Subtree scoping: node 1 covers clusters {1,2} only.
    const std::array<std::byte, 1> only0 = { std::byte{ 0x01 } };
    CHECK( !ml::headnode_visible( w, 1, only0, &lastleaf ));

    // Nothing visible / bad headnode.
    const std::array<std::byte, 1> none = { std::byte{ 0x00 } };
    CHECK( !ml::headnode_visible( w, 0, none, nullptr ));
    CHECK( !ml::headnode_visible( w, -1, all, nullptr ));
    CHECK( !ml::headnode_visible( w, 99, all, nullptr ));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_compress_goldens );
    RUN_TEST( test_compress_run_cap );
    RUN_TEST( test_compress_decompress_roundtrip );
    RUN_TEST( test_build_phs_goldens );
    RUN_TEST( test_build_phs_no_visdata );
    RUN_TEST( test_build_phs_matches_naive_fold );
    RUN_TEST( test_fat_phs_vs_fat_pvs );
    RUN_TEST( test_fat_phs_radius_straddle_and_merge );
    RUN_TEST( test_fat_phs_fallbacks );
    RUN_TEST( test_headnode_visible );

    std::printf( "phs: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
