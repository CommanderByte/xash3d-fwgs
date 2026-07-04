// xash3dpp — BSP on-disk format pins (Chunk 5, C2)
// Verifies the vendored disk structs against common/bspfile.h facts: record
// sizes, field offsets, version fourccs, lump indices, contents values, and
// that a memcpy'd little-endian byte image lands in the right fields.
// Reference: docs/legacy-survey/deep-dive-bsp-loader.md §1-§4.

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/private/map_loader/bsp/disk_format.hpp>

#include "../../test_helpers.hpp"

#include <cstddef>
#include <cstring>

namespace bsp = xash::map_loader::bsp;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// record sizes (byte-frozen)
// ---------------------------------------------------------------------------

static void test_record_sizes()
{
    CHECK_EQ( sizeof( bsp::dlump_t ),       std::size_t{ 8 } );
    CHECK_EQ( sizeof( bsp::dheader_t ),     std::size_t{ 124 } );
    CHECK_EQ( sizeof( bsp::dextrahdr_t ),   std::size_t{ 104 } );
    CHECK_EQ( sizeof( bsp::dmodel_t ),      std::size_t{ 64 } );
    CHECK_EQ( sizeof( bsp::dmiptexlump_t ), std::size_t{ 20 } );
    CHECK_EQ( sizeof( bsp::mip_t ),         std::size_t{ 40 } );
    CHECK_EQ( sizeof( bsp::dvertex_t ),     std::size_t{ 12 } );
    CHECK_EQ( sizeof( bsp::dplane_t ),      std::size_t{ 20 } );
    CHECK_EQ( sizeof( bsp::dnode_t ),       std::size_t{ 24 } );
    CHECK_EQ( sizeof( bsp::dnode32_t ),     std::size_t{ 44 } );
    CHECK_EQ( sizeof( bsp::dleaf_t ),       std::size_t{ 28 } );
    CHECK_EQ( sizeof( bsp::dleaf32_t ),     std::size_t{ 44 } );
    CHECK_EQ( sizeof( bsp::dclipnode_t ),   std::size_t{ 8 } );
    CHECK_EQ( sizeof( bsp::dclipnode32_t ), std::size_t{ 12 } );
    CHECK_EQ( sizeof( bsp::dtexinfo_t ),    std::size_t{ 40 } );
    CHECK_EQ( sizeof( bsp::dfaceinfo_t ),   std::size_t{ 22 } );
    CHECK_EQ( sizeof( bsp::dmarkface_t ),   std::size_t{ 2 } );
    CHECK_EQ( sizeof( bsp::dmarkface32_t ), std::size_t{ 4 } );
    CHECK_EQ( sizeof( bsp::dsurfedge_t ),   std::size_t{ 4 } );
    CHECK_EQ( sizeof( bsp::dedge_t ),       std::size_t{ 4 } );
    CHECK_EQ( sizeof( bsp::dedge32_t ),     std::size_t{ 8 } );
    CHECK_EQ( sizeof( bsp::dface_t ),       std::size_t{ 20 } );
    CHECK_EQ( sizeof( bsp::dface32_t ),     std::size_t{ 28 } );
}

// ---------------------------------------------------------------------------
// version magics + lump directory
// ---------------------------------------------------------------------------

static void test_version_magics()
{
    CHECK_EQ( bsp::k_q1bsp_version, 29 );
    CHECK_EQ( bsp::k_hlbsp_version, 30 );
    // fourccs, little-endian byte order: "BSP2" / "XASH"
    CHECK_EQ( bsp::k_qbsp2_version,   std::int32_t{ 0x32505342 } );
    CHECK_EQ( bsp::k_extra_header_id, std::int32_t{ 0x48534158 } );
    CHECK_EQ( bsp::k_extra_version, 4 );
}

static void test_lump_indices()
{
    CHECK_EQ( bsp::k_lump_entities,     0 );
    CHECK_EQ( bsp::k_lump_planes,       1 );
    CHECK_EQ( bsp::k_lump_textures,     2 );
    CHECK_EQ( bsp::k_lump_vertexes,     3 );
    CHECK_EQ( bsp::k_lump_visibility,   4 );
    CHECK_EQ( bsp::k_lump_nodes,        5 );
    CHECK_EQ( bsp::k_lump_texinfo,      6 );
    CHECK_EQ( bsp::k_lump_faces,        7 );
    CHECK_EQ( bsp::k_lump_lighting,     8 );
    CHECK_EQ( bsp::k_lump_clipnodes,    9 );
    CHECK_EQ( bsp::k_lump_leafs,        10 );
    CHECK_EQ( bsp::k_lump_marksurfaces, 11 );
    CHECK_EQ( bsp::k_lump_edges,        12 );
    CHECK_EQ( bsp::k_lump_surfedges,    13 );
    CHECK_EQ( bsp::k_lump_models,       14 );
    CHECK_EQ( bsp::k_header_lumps,      15 );
    CHECK_EQ( bsp::k_extra_lumps,       12 );
}

// ---------------------------------------------------------------------------
// contents constants (common/const.h:586-603, world.h:23)
// ---------------------------------------------------------------------------

static void test_contents_values()
{
    using namespace xash::map_loader;
    CHECK_EQ( k_contents_none,   0 );
    CHECK_EQ( k_contents_empty, -1 );
    CHECK_EQ( k_contents_solid, -2 );
    CHECK_EQ( k_contents_water, -3 );
    CHECK_EQ( k_contents_slime, -4 );
    CHECK_EQ( k_contents_lava,  -5 );
    CHECK_EQ( k_contents_sky,   -6 );
    CHECK_EQ( k_contents_current_0,    -9 );
    CHECK_EQ( k_contents_current_down, -14 );
    CHECK_EQ( k_contents_translucent,  -15 );
    CHECK_EQ( k_contents_ladder,       -16 );
}

// ---------------------------------------------------------------------------
// memcpy round-trip: a hand-built little-endian byte image lands correctly
// ---------------------------------------------------------------------------

static void test_header_byte_image()
{
    unsigned char raw[sizeof( bsp::dheader_t )] = {};
    // version = 30 (LE)
    raw[0] = 30;
    // lump[1] (planes): fileofs = 0x00000204, filelen = 0x00000140
    raw[4 + 8 * 1 + 0] = 0x04;
    raw[4 + 8 * 1 + 1] = 0x02;
    raw[4 + 8 * 1 + 4] = 0x40;
    raw[4 + 8 * 1 + 5] = 0x01;

    bsp::dheader_t hdr{};
    std::memcpy( &hdr, raw, sizeof hdr );
    CHECK_EQ( hdr.version, 30 );
    CHECK_EQ( hdr.lumps[bsp::k_lump_planes].fileofs, 0x204 );
    CHECK_EQ( hdr.lumps[bsp::k_lump_planes].filelen, 0x140 );
    CHECK_EQ( hdr.lumps[bsp::k_lump_entities].fileofs, 0 );
}

static void test_clipnode_byte_image()
{
    // dclipnode_t: planenum=7, children = {2, -2 (CONTENTS_SOLID)}
    const unsigned char raw[8] = { 7, 0, 0, 0, 0x02, 0x00, 0xFE, 0xFF };
    bsp::dclipnode_t cn{};
    std::memcpy( &cn, raw, sizeof cn );
    CHECK_EQ( cn.planenum, 7 );
    CHECK_EQ( cn.children[0], std::int16_t{ 2 } );
    CHECK_EQ( cn.children[1], std::int16_t{ -2 } );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_record_sizes );
    RUN_TEST( test_version_magics );
    RUN_TEST( test_lump_indices );
    RUN_TEST( test_contents_values );
    RUN_TEST( test_header_byte_image );
    RUN_TEST( test_clipnode_byte_image );

    std::printf( "bsp_disk_format: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
