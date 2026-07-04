// xash3dpp — map checksum goldens (Chunk 5, C6)
// Covers: the singleplayer constant, the multiplayer CRC over lumps 1..14
// (hand-derived golden: pre-final CRC32 of "123456789"), the entities-lump
// exclusion, the missing-final-invert quirk, and checksum() integration
// through load_world_data.
// Legacy reference: mod_bmodel.c CRC32_MapFile (:4093-4165), crclib.c.

#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/map_loader/bsp/map_crc.hpp>

#include "test_bsp_builder.hpp"

#include "../../test_helpers.hpp"

#include <cstring>
#include <vector>

namespace bsp = xash::map_loader::bsp;
namespace ml  = xash::map_loader;
using test_bsp::make_minimal_world;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// hand-derived golden
// ---------------------------------------------------------------------------

static void test_crc_golden_vector()
{
    // Standard (zlib) CRC-32 of "123456789" is 0xCBF43926 AFTER the final
    // xor-invert.  CRC32_MapFile never applies the invert, so the stored
    // value is 0xCBF43926 ^ 0xFFFFFFFF = 0x340BC6D9.
    bsp::dheader_t hdr{};
    hdr.version = bsp::k_hlbsp_version;
    hdr.lumps[bsp::k_lump_planes] = { 124, 9 };

    std::vector<std::byte> file( 124 + 9 );
    std::memcpy( file.data(), &hdr, sizeof hdr );
    std::memcpy( file.data() + 124, "123456789", 9 );

    CHECK_EQ( bsp::map_checksum_multiplayer( file, hdr ), 0x340BC6D9u );
}

static void test_crc_excludes_entities()
{
    bsp::dheader_t hdr{};
    hdr.version = bsp::k_hlbsp_version;
    hdr.lumps[bsp::k_lump_entities] = { 124, 4 };  // EXCLUDED from the CRC
    hdr.lumps[bsp::k_lump_planes]   = { 128, 9 };

    std::vector<std::byte> file( 128 + 9 );
    std::memcpy( file.data(), &hdr, sizeof hdr );
    std::memcpy( file.data() + 124, "XXXX", 4 );
    std::memcpy( file.data() + 128, "123456789", 9 );
    const auto a = bsp::map_checksum_multiplayer( file, hdr );

    std::memcpy( file.data() + 124, "YYYY", 4 ); // entities change → same CRC
    const auto b = bsp::map_checksum_multiplayer( file, hdr );

    CHECK_EQ( a, b );
    CHECK_EQ( a, 0x340BC6D9u );
}

static void test_crc_index_order_not_offset_order()
{
    // Lumps are hashed in INDEX order even when their file offsets are
    // reversed: planes(1)="1234", nodes(5)="56789" but nodes sit first in
    // the file.
    bsp::dheader_t hdr{};
    hdr.version = bsp::k_hlbsp_version;
    hdr.lumps[bsp::k_lump_nodes]  = { 124, 5 };
    hdr.lumps[bsp::k_lump_planes] = { 129, 4 };

    std::vector<std::byte> file( 133 );
    std::memcpy( file.data(), &hdr, sizeof hdr );
    std::memcpy( file.data() + 124, "56789", 5 );
    std::memcpy( file.data() + 129, "1234", 4 );

    CHECK_EQ( bsp::map_checksum_multiplayer( file, hdr ), 0x340BC6D9u );
}

// ---------------------------------------------------------------------------
// integration through load_world_data
// ---------------------------------------------------------------------------

static void test_checksum_options()
{
    const auto file = make_minimal_world().build();

    ml::WorldLoadOptions sp;
    sp.multiplayer_crc = false;
    const auto wsp = ml::load_world_data( file, "t", sp );
    REQUIRE( wsp.has_value() );
    CHECK_EQ( wsp->checksum(), bsp::k_map_crc_singleplayer );
    CHECK_EQ( wsp->checksum(), 0x58415348u ); // "XASH" little-endian

    ml::WorldLoadOptions mp;
    mp.multiplayer_crc = true;
    const auto wmp = ml::load_world_data( file, "t", mp );
    REQUIRE( wmp.has_value() );
    CHECK( wmp->checksum() != bsp::k_map_crc_singleplayer );

    // Deterministic across loads.
    const auto wmp2 = ml::load_world_data( file, "t", mp );
    REQUIRE( wmp2.has_value() );
    CHECK_EQ( wmp->checksum(), wmp2->checksum() );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_crc_golden_vector );
    RUN_TEST( test_crc_excludes_entities );
    RUN_TEST( test_crc_index_order_not_offset_order );
    RUN_TEST( test_checksum_options );

    std::printf( "map_crc: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
