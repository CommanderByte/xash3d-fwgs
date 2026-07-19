// xash3dpp — save/restore codec core tests (Chunk 8, slice S8.1).
// Covers the token hash table (StoreHashTable/BuildHashTable byte-exact ports +
// the HL SDK slot hash), the field-record framing seam (SaveBufferSink write /
// next_field_record pure parse), the SaveBuffer working-buffer lifecycle +
// SAVERESTOREDATA projection, and the engine-owned header structs / descriptor
// tables.  Legacy reference: engine/server/sv_save.c.
//
// Golden vectors are hand-derived from the legacy encoding and their derivation
// is commented at the vector.

#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/token_table.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>
#include <vector>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace abi  = xash::abi;
namespace lim  = xash::limits;
using save::SaveError;

static int g_pass = 0, g_fail = 0;

static mem::PoolHandle g_pool;

// ---------------------------------------------------------------------------
// Compile-time header-layout pins (deep-dive "Header structs" byte layout).
// ---------------------------------------------------------------------------
static_assert( sizeof( save::GameHeader ) == 116 );
static_assert( sizeof( save::SaveHeader ) == 108 );
static_assert( sizeof( save::SaveLightStyle ) == 264 );
static_assert( offsetof( save::SaveHeader, time ) == 16 );
static_assert( offsetof( save::SaveHeader, sky_name ) == 52 );
static_assert( offsetof( save::SaveLightStyle, time ) == 260 );
static_assert( save::k_savefile_magic == 0x564C4156 ); // "VALV" LE
static_assert( save::k_savegame_magic == 0x5641534A ); // "JSAV" LE
static_assert( save::k_savegame_version == 0x0071 );
static_assert( save::k_client_savegame_version == 0x0067 );

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static std::byte b( int v ) noexcept { return static_cast<std::byte>( v & 0xFF ); }

// ---------------------------------------------------------------------------
// Token table
// ---------------------------------------------------------------------------

// Golden StoreHashTable/BuildHashTable format: slot-order NUL-terminated blob,
// a NULL (empty) slot contributing exactly one terminator byte.
static void test_token_blob_golden()
{
    // 3-slot table with slots ["ab", NULL, "c"].  Hand-derived flatten:
    //   'a' 'b' 0   (slot 0)
    //   0           (slot 1 — NULL, just the terminator)
    //   'c' 0       (slot 2)
    const std::array<std::byte, 6> golden = { b( 'a' ), b( 'b' ), b( 0 ),
                                              b( 0 ),
                                              b( 'c' ), b( 0 ) };
    save::TokenTable t( 3 );
    auto             rb = t.rebuild( golden );
    REQUIRE( rb.has_value() );
    CHECK( t.token_at( 0 ) == "ab" );
    CHECK( t.token_at( 1 ).empty() ); // NULL slot
    CHECK( t.token_at( 2 ) == "c" );
    CHECK_EQ( t.flattened_size(), 6u );

    std::array<std::byte, 6> out{};
    auto                     fl = t.flatten( out );
    REQUIRE( fl.has_value() );
    CHECK_EQ( *fl, 6u );
    CHECK( out == golden ); // byte-exact round trip
}

static void test_token_roundtrip()
{
    save::TokenTable  t( lim::save_hash_strings ); // 4095 slots
    const char       *toks[] = { "worldspawn", "trigger_auto", "func_door",
                                 "light", "info_player_start" };
    std::uint16_t     idx[5];

    for ( int i = 0; i < 5; ++i )
    {
        auto r = t.insert( toks[i] );
        REQUIRE( r.has_value() );
        idx[i] = *r;
    }
    // Idempotent re-insert returns the same slot; find() agrees with insert().
    for ( int i = 0; i < 5; ++i )
    {
        auto r = t.insert( toks[i] );
        REQUIRE( r.has_value() );
        CHECK_EQ( *r, idx[i] );
        auto f = t.find( toks[i] );
        REQUIRE( f.has_value() );
        CHECK_EQ( *f, idx[i] );
    }

    // flatten -> rebuild into a fresh table -> identical slot strings + indices.
    std::vector<std::byte> blob( t.flattened_size() );
    auto                   fl = t.flatten( blob );
    REQUIRE( fl.has_value() );
    CHECK_EQ( *fl, blob.size() );

    save::TokenTable t2( lim::save_hash_strings );
    auto             rb = t2.rebuild( blob );
    REQUIRE( rb.has_value() );
    for ( int i = 0; i < 5; ++i )
    {
        CHECK( t2.token_at( idx[i] ) == std::string_view( toks[i] ) );
        auto f = t2.find( toks[i] );
        REQUIRE( f.has_value() );
        CHECK_EQ( *f, idx[i] );
    }
}

static void test_hash_collision()
{
    // 2-slot table: two distinct tokens must land in distinct slots via the
    // linear probe, and both remain findable.
    save::TokenTable t( 2 );
    auto             a = t.insert( "alpha" );
    auto             b_ = t.insert( "bravo" );
    REQUIRE( a.has_value() );
    REQUIRE( b_.has_value() );
    CHECK_NE( *a, *b_ );
    CHECK( *a < 2 );
    CHECK( *b_ < 2 );

    auto fa = t.find( "alpha" );
    auto fb = t.find( "bravo" );
    REQUIRE( fa.has_value() );
    REQUIRE( fb.has_value() );
    CHECK_EQ( *fa, *a );
    CHECK_EQ( *fb, *b_ );
    CHECK( !t.find( "charlie" ).has_value() ); // absent, full probe -> nullopt
}

static void test_token_overflow()
{
    save::TokenTable t( lim::save_hash_strings ); // 4095 slots
    char             name[16];
    for ( int i = 0; i < 4095; ++i )
    {
        std::snprintf( name, sizeof name, "t%d", i );
        auto r = t.insert( name );
        REQUIRE( r.has_value() );
    }
    // Table is now full (load factor 1.0): a new distinct token overflows.
    auto of = t.insert( "overflow_token" );
    CHECK( !of.has_value() );
    CHECK( of.error() == SaveError::TokenOverflow );

    // An already-present token still resolves (match, not overflow).
    auto ex = t.insert( "t0" );
    REQUIRE( ex.has_value() );
}

static void test_token_blob_truncated()
{
    // Blob holds one terminated token but the table expects two -> the 2nd slot
    // scan runs off the end without a terminator.
    const std::array<std::byte, 3> trunc = { b( 'a' ), b( 'b' ), b( 0 ) };
    save::TokenTable               t( 2 );
    auto                           r = t.rebuild( trunc );
    CHECK( !r.has_value() );
    CHECK( r.error() == SaveError::TruncatedBlock );

    // No terminator at all -> truncated on the very first slot.
    const std::array<std::byte, 2> noterm = { b( 'x' ), b( 'y' ) };
    save::TokenTable               t1( 1 );
    auto                           r1 = t1.rebuild( noterm );
    CHECK( !r1.has_value() );
    CHECK( r1.error() == SaveError::TruncatedBlock );

    // Empty blob leaves every slot NULL (legacy `if (tokenSize > 0)` guard).
    save::TokenTable t2( 3 );
    auto             r2 = t2.rebuild( std::span<const std::byte>{} );
    REQUIRE( r2.has_value() );
    CHECK( t2.token_at( 0 ).empty() );
}

// ---------------------------------------------------------------------------
// Field-record framing
// ---------------------------------------------------------------------------

static void test_field_record_golden()
{
    auto buf = save::create_save_buffer( g_pool, 64, 8, 0.0f );
    REQUIRE( buf != nullptr );

    save::SaveBufferSink           sink( *buf );
    const std::array<std::byte, 3> payload = { b( 0xDE ), b( 0xAD ), b( 0xBE ) };
    auto                           w = sink.write_field_record( 0x1234, payload );
    REQUIRE( w.has_value() );

    // Golden: short size=3 (03 00), short token=0x1234 (34 12), payload DE AD BE.
    const std::array<std::byte, 7> golden = { b( 0x03 ), b( 0x00 ), b( 0x34 ),
                                              b( 0x12 ), b( 0xDE ), b( 0xAD ),
                                              b( 0xBE ) };
    auto                           data = buf->data();
    REQUIRE( data.size() == 7 );
    for ( std::size_t i = 0; i < 7; ++i )
        CHECK_EQ( data[i], golden[i] );

    // Pure parse back.
    std::size_t off = 0;
    auto        rec = save::next_field_record( data, off );
    REQUIRE( rec.has_value() );
    CHECK_EQ( rec->token_idx, 0x1234 );
    REQUIRE( rec->payload.size() == 3 );
    CHECK_EQ( rec->payload[0], b( 0xDE ) );
    CHECK_EQ( rec->payload[2], b( 0xBE ) );
    CHECK_EQ( off, 7u );
}

static void test_field_record_roundtrip()
{
    auto buf = save::create_save_buffer( g_pool, 256, 8, 0.0f );
    REQUIRE( buf != nullptr );
    save::SaveBufferSink sink( *buf );

    struct Rec
    {
        std::uint16_t          tok;
        std::vector<std::byte> payload;
    };
    // odd (1, 3), zero, and even (2) payload sizes.
    std::vector<Rec> recs = {
        { 1, { b( 0x01 ) } },
        { 2, { b( 0x02 ), b( 0x03 ), b( 0x04 ) } },
        { 3, {} },
        { 0x0402, { b( 0xAA ), b( 0xBB ) } },
    };

    for ( const auto &r : recs )
    {
        auto w = sink.write_field_record( r.tok, r.payload );
        REQUIRE( w.has_value() );
    }

    auto        data = buf->data();
    std::size_t off  = 0;
    for ( const auto &r : recs )
    {
        auto got = save::next_field_record( data, off );
        REQUIRE( got.has_value() );
        CHECK_EQ( got->token_idx, r.tok );
        REQUIRE( got->payload.size() == r.payload.size() );
        for ( std::size_t i = 0; i < r.payload.size(); ++i )
            CHECK_EQ( got->payload[i], r.payload[i] );
    }
    CHECK_EQ( off, data.size() ); // consumed exactly
}

static void test_field_record_corrupt()
{
    // Truncated header: only 3 bytes present, a record header needs 4.
    const std::array<std::byte, 3> trunc = { b( 0x05 ), b( 0x00 ), b( 0x00 ) };
    {
        std::size_t off = 0;
        auto        r   = save::next_field_record( trunc, off );
        CHECK( !r.has_value() );
        CHECK( r.error() == SaveError::TruncatedBlock );
    }
    // Oversize: header declares size 5 but only 2 payload bytes follow.
    const std::array<std::byte, 6> over = { b( 0x05 ), b( 0x00 ), b( 0x00 ),
                                            b( 0x00 ), b( 0x11 ), b( 0x22 ) };
    {
        std::size_t off = 0;
        auto        r   = save::next_field_record( over, off );
        CHECK( !r.has_value() );
        CHECK( r.error() == SaveError::BadFieldRecord );
    }
}

// ---------------------------------------------------------------------------
// SaveBuffer
// ---------------------------------------------------------------------------

static void test_save_buffer_abi()
{
    auto buf = save::create_save_buffer( g_pool, 1024, lim::save_hash_strings, 42.5f );
    REQUIRE( buf != nullptr );
    CHECK( buf->valid() );
    CHECK_EQ( buf->capacity(), 1024u );
    CHECK_EQ( buf->size(), 0u );

    abi::SAVERESTOREDATA rd{};
    buf->to_abi( rd );
    CHECK( rd.pBaseData != nullptr );
    CHECK( rd.pCurrentData == rd.pBaseData ); // fresh cursor sits at base
    CHECK_EQ( rd.bufferSize, 1024 );
    CHECK_EQ( rd.size, 0 );
    CHECK_EQ( rd.tokenSize, 0 ); // SaveInit shape: token blob not yet flattened
    CHECK_EQ( rd.tokenCount, static_cast<int>( lim::save_hash_strings ) );
    CHECK( rd.pTokens != nullptr );
    CHECK( rd.time == 42.5f );
    CHECK_EQ( rd.connectionCount, 0 ); // landmark/level-list fields zeroed
    CHECK_EQ( rd.tableCount, 0 );
}

static void test_save_buffer_bounds()
{
    auto buf = save::create_save_buffer( g_pool, 8, 4, 0.0f );
    REQUIRE( buf != nullptr );

    const std::array<std::byte, 8> eight{};
    CHECK( buf->write_bytes( eight ).has_value() ); // exactly fills capacity
    CHECK_EQ( buf->size(), 8u );

    const std::array<std::byte, 1> one{};
    auto                           over = buf->write_bytes( one );
    CHECK( !over.has_value() );
    CHECK( over.error() == SaveError::BufferExhausted );
}

static void test_save_buffer_read()
{
    auto buf = save::create_save_buffer( g_pool, 64, 4, 0.0f );
    REQUIRE( buf != nullptr );

    REQUIRE( buf->write_i32( 0x11223344 ).has_value() );
    REQUIRE( buf->write_i16( static_cast<std::int16_t>( 0x5566 ) ).has_value() );
    REQUIRE( buf->write_f32( 1.5f ).has_value() );
    REQUIRE( buf->seek( 0 ).has_value() );

    auto a = buf->read_i32();
    REQUIRE( a.has_value() );
    CHECK_EQ( *a, 0x11223344 );
    auto c = buf->read_i16();
    REQUIRE( c.has_value() );
    CHECK_EQ( static_cast<std::uint16_t>( *c ), 0x5566u );
    auto d = buf->read_f32();
    REQUIRE( d.has_value() );
    CHECK( *d == 1.5f );

    // Read past the valid-data region -> TruncatedBlock.
    auto e = buf->read_i32();
    CHECK( !e.has_value() );
    CHECK( e.error() == SaveError::TruncatedBlock );
}

// ---------------------------------------------------------------------------
// Header descriptor tables
// ---------------------------------------------------------------------------

static void test_header_descriptors()
{
    CHECK_EQ( save::k_game_header_desc.size(), 3u );
    CHECK_EQ( save::k_save_header_desc.size(), 13u );
    CHECK_EQ( save::k_adjacency_desc.size(), 4u );
    CHECK_EQ( save::k_light_style_desc.size(), 3u );
    CHECK_EQ( save::k_entity_table_desc.size(), 5u );

    // Wire field NAMES are the frozen legacy identifiers (token-indexed on disk).
    CHECK_STREQ( save::k_game_header_desc[2].fieldName, "mapCount" );
    CHECK_STREQ( save::k_save_header_desc[4].fieldName, "time" );
    CHECK( save::k_save_header_desc[4].fieldType == abi::FIELD_TIME );

    // ENTITYTABLE excludes `pent`: classname is the 5th (last) serialized field.
    CHECK_STREQ( save::k_entity_table_desc[4].fieldName, "classname" );
    CHECK( save::k_entity_table_desc[4].fieldType == abi::FIELD_STRING );

    // Arch-independent header offset pins.
    CHECK_EQ( save::k_save_header_desc[0].fieldOffset, 0 );
    CHECK_EQ( save::k_game_header_desc[2].fieldOffset, 112 );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // Every mutating save entry point asserts ThreadRole::Main; register it so
    // the bare asserts do not FATAL the harness (server/map_loader test idiom).
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    g_pool = mem::create_pool( "save_test" );

    RUN_TEST( test_token_blob_golden );
    RUN_TEST( test_token_roundtrip );
    RUN_TEST( test_hash_collision );
    RUN_TEST( test_token_overflow );
    RUN_TEST( test_token_blob_truncated );
    RUN_TEST( test_field_record_golden );
    RUN_TEST( test_field_record_roundtrip );
    RUN_TEST( test_field_record_corrupt );
    RUN_TEST( test_save_buffer_abi );
    RUN_TEST( test_save_buffer_bounds );
    RUN_TEST( test_save_buffer_read );
    RUN_TEST( test_header_descriptors );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "save_codec: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
