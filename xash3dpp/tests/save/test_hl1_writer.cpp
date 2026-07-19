// xash3dpp — SaveGameState `.HL1` per-level writer tests (Chunk 8, slice S8.3).
// Covers LevelStateWriter: the full `.HL1` image assembly (preamble + token blob
// + ETABLE region + data region), the engine-owned descriptor-block writers
// (Save Header / ADJACENCY / LIGHTSTYLE), the pfnSave-stand-in IEntitySaver seam
// + location/size/FENTTABLE_PLAYER bookkeeping, a reader-side round-trip smoke,
// and preamble corruption rejection.  Legacy reference: engine/server/sv_save.c
// :1469-1619 (SaveGameState), :854-958 (LoadSaveData envelope).
//
// Golden derivation: byte vectors are hand-derived from the legacy encoding
// rules (settled by the S8.1/S8.2 parity gates — field values inline, all-zero
// fields skipped, token table carries only NAMES, block header = the post-skip
// actual field count).  The token-table slot assignments are computed from the
// documented hash (token_table.hpp hash_string: rotr4-xor-accumulate) simulated
// over the exact interning order; each golden lists its token->slot map.
//
// Two goldens are fully hex-pinned:
//   • the EMPTY case (0 connections, 0 lightstyles, 1 entity) — the whole
//     194-byte image, the byte-exact anchor;
//   • the MINIMAL case (1 conn, 2 lightstyles, 2 entities) Save Header + ADJACENCY
//     sub-blocks — pinned byte-for-byte to exercise FIELD_INTEGER/TIME/CHARACTER/
//     VECTOR + the FIELD_EDICT NULL-skip.
// The MINIMAL case's bulk (256-byte lightstyle style[] arrays, entity payloads)
// is validated by a full reader-side round-trip rather than a giant, unreviewable
// hex blob — the token-blob flatten format itself is already golden-pinned in
// test_save_codec (S8.1), so re-pinning it here adds no coverage.

#include <xash3dpp/private/save/descriptor_codec.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/level_state_writer.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/token_table.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace abi  = xash::abi;

static int g_pass = 0, g_fail = 0;

static mem::PoolHandle g_pool;

static std::byte b( int v ) noexcept { return static_cast<std::byte>( v & 0xFF ); }

template<std::size_t N>
static void set_arr( char ( &dst )[N], std::string_view s ) noexcept
{
    std::memset( dst, 0, N );
    const std::size_t n = ( s.size() < N ) ? s.size() : ( N - 1 );
    if ( n )
        std::memcpy( dst, s.data(), n );
}

// The pfnSave stand-in: append one deterministic field record (name + AABBCCDD)
// per entity, exactly as the game-DLL DispatchSave would emit real field records.
struct FakeSaver final : save::IEntitySaver
{
    std::string_view field_name;
    explicit FakeSaver( std::string_view n ) noexcept : field_name( n ) {}

    save::Result<void> save_entity( std::size_t, const abi::edict_t *, save::IFieldSink &sink,
                                    save::TokenTable &tokens ) noexcept override
    {
        auto tok = tokens.insert( field_name );
        if ( !tok )
            return std::unexpected( tok.error() );
        const std::array<std::byte, 4> payload = { b( 0xAA ), b( 0xBB ), b( 0xCC ), b( 0xDD ) };
        return sink.write_field_record( *tok, payload );
    }
};

// ---------------------------------------------------------------------------
// EMPTY case — full 194-byte image, byte-exact
// ---------------------------------------------------------------------------
//
// Inputs: skill=0 (skip), time=1.0f, mapName="t0", no sky, 0 connections,
// 0 lightstyles, 1 valid non-client entity, classname "a", saver writes field
// "x"=AABBCCDD.  token table size = 11 slots (tokenCount=11).
//
// Non-empty Save Header fields (D2 skip): entityCount(1), time(1.0), mapName ->
// actualCount=3 (skillLevel/connectionCount/lightStyleCount/sky* are all-zero).
// Token interning order (block name then each WRITTEN field name):
//   Save Header, entityCount, time, mapName, x, ETABLE, location, size, classname
// hash_string(name) % 11 + linear probe (11 slots, no collisions):
//   entityCount->0  mapName->1  ETABLE->2  location->3  size->4  classname->5
//   Save Header->8  time->9     x->10       (slots 6,7 empty)
// ETABLE row 0: id=0 (skip), location=60 (0x3C), size=8, flags=0 (skip),
//   classname="a" -> actualCount=3.  location=60 = header block byte size.
static void test_empty_full_image()
{
    abi::edict_t e0{}; // free==0 -> valid; no FL_CLIENT
    std::array<abi::edict_t *, 1> edicts   = { &e0 };
    std::array<std::string_view, 1> classes = { "a" };

    FakeSaver saver( "x" );

    save::LevelStateParams p;
    p.skill_level = 0;
    p.time        = 1.0f;
    p.map_name    = "t0";
    p.edicts      = edicts;
    p.classnames  = classes;
    p.saver       = &saver;

    auto buf = save::create_save_buffer( g_pool, 1024, 11, 0.0f );
    REQUIRE( buf != nullptr );
    save::EntityTable table;
    save::LevelStateWriter writer( *buf, table );

    std::vector<std::byte> image;
    auto w = writer.write( p, image );
    REQUIRE( w.has_value() );

    const std::array<std::byte, 194> golden = {
        // -- preamble (24): VALV, 0x71, size=98, tableCount=1, tokenCount=11, tokenSize=72
        b( 0x56 ), b( 0x41 ), b( 0x4C ), b( 0x56 ), b( 0x71 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x62 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x0B ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x48 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        // -- token blob (72), slot order 0..10
        b( 0x65 ), b( 0x6E ), b( 0x74 ), b( 0x69 ), b( 0x74 ), b( 0x79 ), b( 0x43 ), b( 0x6F ),
        b( 0x75 ), b( 0x6E ), b( 0x74 ), b( 0x00 ), b( 0x6D ), b( 0x61 ), b( 0x70 ), b( 0x4E ),
        b( 0x61 ), b( 0x6D ), b( 0x65 ), b( 0x00 ), b( 0x45 ), b( 0x54 ), b( 0x41 ), b( 0x42 ),
        b( 0x4C ), b( 0x45 ), b( 0x00 ), b( 0x6C ), b( 0x6F ), b( 0x63 ), b( 0x61 ), b( 0x74 ),
        b( 0x69 ), b( 0x6F ), b( 0x6E ), b( 0x00 ), b( 0x73 ), b( 0x69 ), b( 0x7A ), b( 0x65 ),
        b( 0x00 ), b( 0x63 ), b( 0x6C ), b( 0x61 ), b( 0x73 ), b( 0x73 ), b( 0x6E ), b( 0x61 ),
        b( 0x6D ), b( 0x65 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x53 ), b( 0x61 ), b( 0x76 ),
        b( 0x65 ), b( 0x20 ), b( 0x48 ), b( 0x65 ), b( 0x61 ), b( 0x64 ), b( 0x65 ), b( 0x72 ),
        b( 0x00 ), b( 0x74 ), b( 0x69 ), b( 0x6D ), b( 0x65 ), b( 0x00 ), b( 0x78 ), b( 0x00 ),
        // -- ETABLE region (30): row0 { location=60, size=8, classname="a\0" }
        b( 0x04 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x3C ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x08 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x02 ), b( 0x00 ), b( 0x05 ), b( 0x00 ), b( 0x61 ), b( 0x00 ),
        // -- data region (68): Save Header block (60) + entity0 rec("x",AABBCCDD)
        b( 0x04 ), b( 0x00 ), b( 0x08 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x09 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x80 ), b( 0x3F ),
        b( 0x20 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x74 ), b( 0x30 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x0A ), b( 0x00 ),
        b( 0xAA ), b( 0xBB ), b( 0xCC ), b( 0xDD ),
    };

    REQUIRE( image.size() == golden.size() );
    for ( std::size_t i = 0; i < golden.size(); ++i )
        CHECK_EQ( image[i], golden[i] );

    // ETABLE row bookkeeping also lives on the borrowed table.
    CHECK_EQ( table.count(), 1u );
    CHECK_EQ( table.row( 0 ).location, 60 );
    CHECK_EQ( table.row( 0 ).size, 8 );
    CHECK( table.classname( 0 ) == "a" );
}

// ---------------------------------------------------------------------------
// MINIMAL case — shared builder
// ---------------------------------------------------------------------------
//
// 1 connection, 2 lightstyles, 2 entities (entity 1 is a client -> FENTTABLE_
// PLAYER).  skill=0 (skip).  token table = 24 slots.
struct MinimalScene
{
    abi::edict_t                    e0{}, e1{};
    std::array<abi::edict_t *, 2>   edicts{};
    std::array<std::string_view, 2> classes = { "info", "player" };
    abi::LEVELLIST                  conn{};
    std::array<save::LightStyleInput, 2> lights{};
    FakeSaver                       saver{ "v" };
    save::LevelStateParams          p{};

    MinimalScene() noexcept
    {
        e1.v.flags = abi::k_fl_client;    // FENTTABLE_PLAYER on row 1
        edicts     = { &e0, &e1 };

        set_arr( conn.mapName, "c1a1" );
        set_arr( conn.landmarkName, "lm1" );
        conn.vecLandmarkOrigin[0] = 1.0f; // {1,0,0} -> FIELD_VECTOR written
        conn.pentLandmark         = nullptr; // FIELD_EDICT NULL -> DataEmpty-skipped

        lights[0] = { 1, "m", 5.0f };
        lights[1] = { 2, "a", 6.0f };

        p.skill_level = 0;
        p.time        = 30.0f;
        p.map_name    = "c1a0";
        p.connections = std::span<const abi::LEVELLIST>( &conn, 1 );
        p.lightstyles = lights;
        p.edicts      = edicts;
        p.classnames  = classes;
        p.saver       = &saver;
    }
};

// Save Header block golden (76 bytes) — token slots (24 slots): Save Header=20,
// entityCount=8, connectionCount=0, lightStyleCount=3, time=11, mapName=4.
// 5 non-empty fields: entityCount=2, connectionCount=1, lightStyleCount=2,
// time=30.0 (0x41F00000), mapName="c1a0".
static const std::array<std::byte, 76> k_min_save_header = {
    b( 0x04 ), b( 0x00 ), b( 0x14 ), b( 0x00 ), b( 0x05 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x04 ), b( 0x00 ), b( 0x08 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x04 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x04 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x04 ), b( 0x00 ), b( 0x0B ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0xF0 ), b( 0x41 ),
    b( 0x20 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x63 ), b( 0x31 ), b( 0x61 ), b( 0x30 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
};

// ADJACENCY block golden (96 bytes) — token slots: ADJACENCY=5, mapName=4 (dup),
// landmarkName=1, vecLandmarkOrigin=2.  3 non-empty fields (pentLandmark NULL ->
// skipped): mapName[32]="c1a1", landmarkName[32]="lm1", vec=12B {1,0,0}.
static const std::array<std::byte, 96> k_min_adjacency = {
    b( 0x04 ), b( 0x00 ), b( 0x05 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x20 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x63 ), b( 0x31 ), b( 0x61 ), b( 0x31 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x20 ), b( 0x00 ), b( 0x01 ), b( 0x00 ),
    b( 0x6C ), b( 0x6D ), b( 0x31 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    b( 0x0C ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x80 ), b( 0x3F ),
    b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
};

// ---------------------------------------------------------------------------
// MINIMAL — preamble + assembly layout
// ---------------------------------------------------------------------------
static void test_minimal_preamble_and_layout()
{
    MinimalScene s;
    auto         buf = save::create_save_buffer( g_pool, 4096, 24, 0.0f );
    REQUIRE( buf != nullptr );
    save::EntityTable      table;
    save::LevelStateWriter writer( *buf, table );

    std::vector<std::byte> image;
    REQUIRE( writer.write( s.p, image ).has_value() );

    // Preamble magic + version pinned exactly (VALV / 0x0071).
    CHECK_EQ( image[0], b( 0x56 ) );
    CHECK_EQ( image[1], b( 0x41 ) );
    CHECK_EQ( image[2], b( 0x4C ) );
    CHECK_EQ( image[3], b( 0x56 ) );
    CHECK_EQ( image[4], b( 0x71 ) );

    auto pre = save::parse_hl1_preamble( image );
    REQUIRE( pre.has_value() );
    CHECK_EQ( pre->table_count, 2 );  // 2 entities
    CHECK_EQ( pre->token_count, 24 ); // slot count == SaveBuffer token table size

    // Region math: total == preamble + tokens + (ETABLE+data == size).
    CHECK_EQ( image.size(), save::k_hl1_preamble_bytes +
                                static_cast<std::size_t>( pre->token_size ) +
                                static_cast<std::size_t>( pre->size ) );
    // tokenSize matches what a fresh flatten of the writer's token table emits.
    CHECK_EQ( static_cast<std::size_t>( pre->token_size ), buf->tokens().flattened_size() );

    // Assembly ORDER: tokens region begins immediately after the 24B preamble.
    CHECK_EQ( pre->token_offset(), 24u );
    CHECK_EQ( pre->etable_offset(), 24u + static_cast<std::size_t>( pre->token_size ) );
}

// ---------------------------------------------------------------------------
// MINIMAL — Save Header + ADJACENCY sub-blocks byte-exact
// ---------------------------------------------------------------------------
static void test_minimal_blocks_golden()
{
    MinimalScene s;
    auto         buf = save::create_save_buffer( g_pool, 4096, 24, 0.0f );
    REQUIRE( buf != nullptr );
    save::EntityTable      table;
    save::LevelStateWriter writer( *buf, table );

    std::vector<std::byte> image;
    REQUIRE( writer.write( s.p, image ).has_value() );

    auto pre = save::parse_hl1_preamble( image );
    REQUIRE( pre.has_value() );

    // Locate the data region: rebuild tokens, then consume the ETABLE region
    // (its size is NOT stored on disk — parse tableCount blocks to find where the
    // data region begins).
    const std::span<const std::byte> img{ image };
    save::TokenTable tokens( static_cast<std::size_t>( pre->token_count ) );
    REQUIRE( tokens
                 .rebuild( img.subspan( pre->token_offset(),
                                        static_cast<std::size_t>( pre->token_size ) ) )
                 .has_value() );

    save::EntityTable etbl;
    etbl.init( static_cast<std::size_t>( pre->table_count ) );
    std::size_t off = pre->etable_offset();
    REQUIRE( etbl.deserialize( img, off, tokens ).has_value() );
    const std::size_t data_offset = off; // ETABLE consumed -> data region start

    // Save Header block is the first block in the data region.
    for ( std::size_t i = 0; i < k_min_save_header.size(); ++i )
        CHECK_EQ( image[data_offset + i], k_min_save_header[i] );

    // ADJACENCY block follows immediately.
    const std::size_t adj_offset = data_offset + k_min_save_header.size();
    for ( std::size_t i = 0; i < k_min_adjacency.size(); ++i )
        CHECK_EQ( image[adj_offset + i], k_min_adjacency[i] );
}

// ---------------------------------------------------------------------------
// MINIMAL — full reader-side round-trip smoke
// ---------------------------------------------------------------------------
static void test_minimal_roundtrip()
{
    MinimalScene s;
    auto         buf = save::create_save_buffer( g_pool, 4096, 24, 0.0f );
    REQUIRE( buf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *buf, wtable );

    std::vector<std::byte> image;
    REQUIRE( writer.write( s.p, image ).has_value() );

    auto pre = save::parse_hl1_preamble( image );
    REQUIRE( pre.has_value() );

    // 1. token rebuild (S8.1)
    const std::span<const std::byte> img{ image };
    save::TokenTable tokens( static_cast<std::size_t>( pre->token_count ) );
    REQUIRE( tokens
                 .rebuild( img.subspan( pre->token_offset(),
                                        static_cast<std::size_t>( pre->token_size ) ) )
                 .has_value() );

    // 2. ETABLE region (S8.2)
    save::EntityTable rtable;
    rtable.init( static_cast<std::size_t>( pre->table_count ) );
    std::size_t off = pre->etable_offset();
    REQUIRE( rtable.deserialize( img, off, tokens ).has_value() );
    const std::size_t data_offset = off;

    // Row 0: id=0 (skipped, survives via init), non-client; row 1: id=1, client.
    CHECK_EQ( rtable.row( 0 ).id, 0 );
    CHECK( rtable.classname( 0 ) == "info" );
    CHECK_EQ( rtable.row( 1 ).id, 1 );
    CHECK( rtable.classname( 1 ) == "player" );
    CHECK( ( static_cast<unsigned>( rtable.row( 1 ).flags ) & abi::k_fenttable_player ) != 0 );
    CHECK( ( static_cast<unsigned>( rtable.row( 0 ).flags ) & abi::k_fenttable_player ) == 0 );

    // 3. Save Header (descriptor overlay read)
    save::SaveHeader hdr{};
    REQUIRE(
        save::read_descriptor_block( img, off, tokens, "Save Header", &hdr,
                                     save::k_save_header_desc, 0.0f )
            .has_value() );
    CHECK_EQ( hdr.entity_count, 2 );
    CHECK_EQ( hdr.connection_count, 1 );
    CHECK_EQ( hdr.light_style_count, 2 );
    CHECK( hdr.time == 30.0f );
    CHECK_STREQ( hdr.map_name, "c1a0" );
    CHECK_EQ( hdr.skill_level, 0 ); // skipped on the wire, overlay left it at init

    // 4. ADJACENCY x connection_count
    for ( int i = 0; i < hdr.connection_count; ++i )
    {
        abi::LEVELLIST conn{};
        REQUIRE( save::read_descriptor_block( img, off, tokens, "ADJACENCY", &conn,
                                              save::k_adjacency_desc, 0.0f )
                     .has_value() );
        CHECK_STREQ( conn.mapName, "c1a1" );
        CHECK_STREQ( conn.landmarkName, "lm1" );
        CHECK( conn.vecLandmarkOrigin[0] == 1.0f );
        CHECK( conn.vecLandmarkOrigin[1] == 0.0f );
        CHECK( conn.pentLandmark == nullptr ); // FIELD_EDICT never read (skipped)
    }

    // 5. LIGHTSTYLE x light_style_count
    const int   expected_index[2] = { 1, 2 };
    const char *expected_style[2] = { "m", "a" };
    const float expected_time[2]  = { 5.0f, 6.0f };
    for ( int i = 0; i < hdr.light_style_count; ++i )
    {
        save::SaveLightStyle light{};
        REQUIRE( save::read_descriptor_block( img, off, tokens, "LIGHTSTYLE", &light,
                                              save::k_light_style_desc, 0.0f )
                     .has_value() );
        CHECK_EQ( light.index, expected_index[i] );
        CHECK_STREQ( light.style, expected_style[i] );
        CHECK( light.time == expected_time[i] );
    }

    // 6. offset now sits at the first entity payload; entity 0's ETABLE.location
    // is measured from the data-region base, so it must match exactly.
    CHECK_EQ( off - data_offset, static_cast<std::size_t>( rtable.row( 0 ).location ) );
}

// ---------------------------------------------------------------------------
// Entity-loop bookkeeping: location/size deltas, FENTTABLE_PLAYER, invalid skip
// ---------------------------------------------------------------------------
static void test_entity_bookkeeping()
{
    // 3 entities: valid non-client, INVALID (free), valid client.
    abi::edict_t e0{}, e1{}, e2{};
    e1.free    = 1;                 // SV_IsValidEdict -> false, pfnSave skipped
    e2.v.flags = abi::k_fl_client;  // FENTTABLE_PLAYER
    std::array<abi::edict_t *, 3>   edicts  = { &e0, &e1, &e2 };
    std::array<std::string_view, 3> classes = { "c0", "c1", "c2" };

    FakeSaver saver( "v" );
    save::LevelStateParams p;
    p.time       = 10.0f;
    p.map_name   = "m";
    p.edicts     = edicts;
    p.classnames = classes;
    p.saver      = &saver;

    auto buf = save::create_save_buffer( g_pool, 2048, 32, 0.0f );
    REQUIRE( buf != nullptr );
    save::EntityTable      table;
    save::LevelStateWriter writer( *buf, table );

    std::vector<std::byte> image;
    REQUIRE( writer.write( p, image ).has_value() );

    REQUIRE( table.count() == 3u );
    // Each valid entity's payload is rec("v", 4 bytes) = 8 bytes.
    CHECK_EQ( table.row( 0 ).size, 8 );
    CHECK_EQ( table.row( 1 ).size, 0 ); // invalid -> pfnSave skipped, size stays 0
    CHECK_EQ( table.row( 2 ).size, 8 );

    // location is monotone; the invalid row consumes no bytes, so rows 1 and 2
    // share the same base cursor (row 0 end == row 1 loc == row 2 loc).
    CHECK_EQ( table.row( 1 ).location, table.row( 0 ).location + 8 );
    CHECK_EQ( table.row( 2 ).location, table.row( 1 ).location );

    // FL_CLIENT tagging (only the valid client row).
    CHECK( ( static_cast<unsigned>( table.row( 0 ).flags ) & abi::k_fenttable_player ) == 0 );
    CHECK( ( static_cast<unsigned>( table.row( 2 ).flags ) & abi::k_fenttable_player ) != 0 );

    // The invalid entity never had its classname set (DispatchSave didn't run).
    CHECK( table.classname( 1 ).empty() );
    CHECK( table.classname( 0 ) == "c0" );
    CHECK( table.classname( 2 ) == "c2" );
}

// ---------------------------------------------------------------------------
// Corrupt / truncated preamble -> specific SaveError
// ---------------------------------------------------------------------------
static void test_corrupt_preamble()
{
    // Build one good image to mutate.
    abi::edict_t e0{};
    std::array<abi::edict_t *, 1>   edicts  = { &e0 };
    std::array<std::string_view, 1> classes = { "a" };
    FakeSaver                       saver( "x" );
    save::LevelStateParams          p;
    p.time = 1.0f; p.map_name = "t0"; p.edicts = edicts; p.classnames = classes; p.saver = &saver;

    auto buf = save::create_save_buffer( g_pool, 1024, 16, 0.0f );
    REQUIRE( buf != nullptr );
    save::EntityTable      table;
    save::LevelStateWriter writer( *buf, table );
    std::vector<std::byte> good;
    REQUIRE( writer.write( p, good ).has_value() );

    // A well-formed image parses.
    REQUIRE( save::parse_hl1_preamble( good ).has_value() );

    // 1. Truncated below the 24-byte preamble.
    {
        auto r = save::parse_hl1_preamble( std::span<const std::byte>( good ).subspan( 0, 20 ) );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::TruncatedBlock );
    }
    // 2. Bad magic.
    {
        std::vector<std::byte> bad = good;
        bad[0] = b( 0xFF );
        auto r = save::parse_hl1_preamble( bad );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::BadMagic );
    }
    // 3. Bad version (byte 4).
    {
        std::vector<std::byte> bad = good;
        bad[4] = b( 0x70 ); // 0x0070 != 0x0071
        auto r = save::parse_hl1_preamble( bad );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::VersionMismatch );
    }
    // 4. Over-budget tokenCount (> SAVE_HASHSTRINGS) -> CorruptHeader.
    {
        std::vector<std::byte> bad = good;
        bad[16] = b( 0x00 ); bad[17] = b( 0x10 ); bad[18] = b( 0x00 ); bad[19] = b( 0x00 ); // 0x1000 = 4096
        auto r = save::parse_hl1_preamble( bad );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::CorruptHeader );
    }
    // 5. Declared regions exceed the image length -> TruncatedBlock.
    {
        std::vector<std::byte> bad = good;
        // inflate `size` (bytes 8..11) far beyond the actual buffer.
        bad[8] = b( 0xFF ); bad[9] = b( 0xFF ); bad[10] = b( 0x00 ); bad[11] = b( 0x00 );
        auto r = save::parse_hl1_preamble( bad );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::TruncatedBlock );
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    g_pool = mem::create_pool( "hl1_writer_test" );

    RUN_TEST( test_empty_full_image );
    RUN_TEST( test_minimal_preamble_and_layout );
    RUN_TEST( test_minimal_blocks_golden );
    RUN_TEST( test_minimal_roundtrip );
    RUN_TEST( test_entity_bookkeeping );
    RUN_TEST( test_corrupt_preamble );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "hl1_writer: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
