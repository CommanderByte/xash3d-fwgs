// xash3dpp — ENTITYTABLE build/read layer tests (Chunk 8, slice S8.2).
// Covers EntityTable::init (InitEntityTable parity), edict_from (EdictFromTable
// clamp quirk), and the per-row ETABLE wire block serialize()/deserialize()
// (golden + round-trip + corrupt input + a hand-built legacy-shape cross
// check). Legacy reference: engine/server/sv_save.c — see entity_table.hpp
// for the exact citations and deviation notes.
//
// D1/D2/D3 (2026-07-19 S8.2 parity audit, fixed pre-commit):
//   D1 — FIELD_STRING (classname) payload is the raw NUL-terminated TEXT,
//        not a token-table index; the token table indexes NAMES only.
//   D2 — HL-SDK CSave::WriteFields DataEmpty semantics: an all-zero/empty
//        field is omitted from the stream entirely.
//   D3 — the block header's field count is the number of fields ACTUALLY
//        emitted after the D2 skip, not the fixed descriptor size (5).
// `entry_for` (pent -> row scan) was deleted per the audit's advisory: no
// engine-side consumer exists in this repo (see entity_table.hpp history).

#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/token_table.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace abi  = xash::abi;

static int g_pass = 0, g_fail = 0;

static mem::PoolHandle g_pool;

static std::byte b( int v ) noexcept { return static_cast<std::byte>( v & 0xFF ); }

// ---------------------------------------------------------------------------
// init() — InitEntityTable parity (sv_save.c:390-405)
// ---------------------------------------------------------------------------

static void test_init_parity()
{
    save::EntityTable table;
    table.init( 4 );

    CHECK_EQ( table.count(), 4u );
    for ( std::size_t i = 0; i < 4; ++i )
    {
        const auto &row = table.row( i );
        CHECK_EQ( row.id, static_cast<int>( i ) );
        CHECK( row.pent == nullptr );        // this component has no edict arena — see header doc
        CHECK_EQ( row.location, 0 );
        CHECK_EQ( row.size, 0 );
        CHECK_EQ( row.flags, 0 );
        CHECK_EQ( row.classname, 0 );        // raw string_t untouched by init()
        CHECK( table.classname( i ).empty() );
    }

    // Re-init to a smaller count reuses the object and re-zeroes cleanly.
    table.init( 1 );
    CHECK_EQ( table.count(), 1u );
    CHECK_EQ( table.row( 0 ).id, 0 );
}

// ---------------------------------------------------------------------------
// edict_from() — EdictFromTable clamp quirk (sv_save.c:434-443)
// ---------------------------------------------------------------------------

static void test_edict_from_clamp()
{
    std::array<abi::edict_t, 4> edicts{};

    save::EntityTable table;
    table.init( 4 );
    for ( std::size_t i = 0; i < 4; ++i )
        table.row( i ).pent = &edicts[i];

    // In-range: direct index.
    CHECK( table.edict_from( 0 ) == &edicts[0] );
    CHECK( table.edict_from( 3 ) == &edicts[3] );
    CHECK( table.edict_from( 2 ) == &edicts[2] );

    // Out-of-range CLAMP (not modulo): negative saturates to 0, too-large
    // saturates to count()-1 == 3. A modulo would give edict_from(-5) ==
    // index 3 (since -5 mod 4 == 3 in a wrapping scheme) or edict_from(100)
    // == index 0 (100 mod 4 == 0) — neither matches what bound() computes.
    CHECK( table.edict_from( -5 ) == &edicts[0] );
    CHECK( table.edict_from( -1 ) == &edicts[0] );
    CHECK( table.edict_from( 100 ) == &edicts[3] );
    CHECK( table.edict_from( 4 ) == &edicts[3] );   // one past the last valid index

    // Deviation from legacy UB: an empty table (tableCount == 0) returns
    // nullptr instead of indexing pTable[-1].
    save::EntityTable empty_table;
    CHECK_EQ( empty_table.count(), 0u );
    CHECK( empty_table.edict_from( 0 ) == nullptr );
    CHECK( empty_table.edict_from( 5 ) == nullptr );
}

// ---------------------------------------------------------------------------
// serialize() — hex-pinned golden for one row (corrected D1/D2/D3 codec)
// ---------------------------------------------------------------------------

// Golden derivation: a single ETABLE row (id=1, location=0, size=0, flags=0,
// classname="ab") serialized against an 8-slot TokenTable.
//
// D2 (DataEmpty skip): location/size/flags are all-zero int32s -> OMITTED
// from the stream entirely. id=1 is non-zero -> written. classname="ab" is
// non-empty -> written. So only 2 of the 5 descriptor fields are actually
// emitted: id, classname.
//
// D3 (actualCount): the block header's field count is therefore 2, not the
// fixed descriptor size (5).
//
// D1 (FIELD_STRING payload): classname's VALUE payload is the raw text
// bytes INCLUDING the trailing '\0' ("ab" -> 3 bytes: 0x61 0x62 0x00), not a
// token-table index. The token table indexes NAMES only.
//
// Token interning: a field's NAME token is interned only at the moment it
// is written (legacy: TokenHash fires from inside BufferField), so a
// SKIPPED field's name is NEVER inserted — "location"/"size"/"flags" are
// not in the table at all for this row. Insertion order is therefore just:
// block name, then each WRITTEN field's name, in descriptor order:
//   1. "ETABLE"    (block header, always written)
//   2. "id"        (written: id=1 != 0)
//   3. "classname" (written: classname="ab" non-empty)
// Slot assignment via the documented hash formula (token_table.hpp
// hash_string: rotr4-xor-accumulate) with an 8-slot table, no collisions:
//   hash("ETABLE")   %8=1 -> slot 1 (free)
//   hash("id")       %8=2 -> slot 2 (free)
//   hash("classname")%8=0 -> slot 0 (free)
// So token indices are: classname=0, ETABLE=1, id=2.
static void test_row_golden()
{
    save::TokenTable  tokens( 8 );
    save::EntityTable table;
    table.init( 1 );
    table.row( 0 ).id = 1; // already 1 from init(), set explicitly for clarity
    table.set_classname( 0, "ab" );

    auto buf = save::create_save_buffer( g_pool, 128, 8, 0.0f );
    REQUIRE( buf != nullptr );
    save::SaveBufferSink sink( *buf );

    auto w = table.serialize( sink, tokens );
    REQUIRE( w.has_value() );

    const std::array<std::byte, 23> golden = {
        // block header: field-record{ size=4, token(ETABLE)=1, payload=int32(actualCount=2) }
        b( 0x04 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        // id: { size=4, token(id)=2, payload=int32(1) }
        b( 0x04 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        // classname: { size=3, token(classname)=0, payload=text("ab")+NUL = 61 62 00 }
        b( 0x03 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x61 ), b( 0x62 ), b( 0x00 ),
    };

    auto data = buf->data();
    REQUIRE( data.size() == golden.size() );
    for ( std::size_t i = 0; i < golden.size(); ++i )
        CHECK_EQ( data[i], golden[i] );

    // Parse the golden bytes back with the SAME token table (the real
    // pipeline flattens+rebuilds the token table before field parsing;
    // that round trip is already proven in test_save_codec — this test
    // isolates the ETABLE row codec itself). init() before deserialize()
    // matters here too (entity_table.hpp deserialize() doc).
    save::EntityTable table2;
    table2.init( 1 );
    std::size_t off = 0;
    auto        r   = table2.deserialize( data, off, tokens );
    REQUIRE( r.has_value() );
    CHECK_EQ( off, data.size() );
    CHECK_EQ( table2.row( 0 ).id, 1 );
    CHECK_EQ( table2.row( 0 ).location, 0 );
    CHECK_EQ( table2.row( 0 ).size, 0 );
    CHECK_EQ( table2.row( 0 ).flags, 0 );
    CHECK( table2.row( 0 ).pent == nullptr ); // never read from disk
    CHECK( table2.classname( 0 ) == "ab" );
}

// ---------------------------------------------------------------------------
// serialize() — a fully-zero row emits header{count=0} and NOTHING else (D2/D3)
// ---------------------------------------------------------------------------

static void test_fully_zero_row()
{
    // Row 0 of a freshly init()'d table: id=0 (index default), location=0,
    // size=0, flags=0, classname="" — every field is at its DataEmpty zero,
    // so ALL FIVE are omitted; only the block header (actualCount=0) is
    // written.
    save::TokenTable  tokens( 8 );
    save::EntityTable table;
    table.init( 1 );

    auto buf = save::create_save_buffer( g_pool, 64, 8, 0.0f );
    REQUIRE( buf != nullptr );
    save::SaveBufferSink sink( *buf );
    REQUIRE( table.serialize( sink, tokens ).has_value() );

    // Exactly 8 bytes: { size=4, token(ETABLE)=1 (sole interned name in a
    // fresh table), payload=int32(0) }. No field records follow.
    const std::array<std::byte, 8> golden = {
        b( 0x04 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    };
    auto data = buf->data();
    REQUIRE( data.size() == golden.size() );
    for ( std::size_t i = 0; i < golden.size(); ++i )
        CHECK_EQ( data[i], golden[i] );

    // Round trip: deserialize() must leave the pre-initialized (id=0) row
    // intact rather than resetting it via a fresh zero-initialized struct
    // (entity_table.hpp deserialize() doc — the init-then-deserialize
    // ordering this depends on).
    save::EntityTable readback;
    readback.init( 1 );
    std::size_t off = 0;
    auto        r   = readback.deserialize( data, off, tokens );
    REQUIRE( r.has_value() );
    CHECK_EQ( off, data.size() );
    CHECK_EQ( readback.row( 0 ).id, 0 );
    CHECK_EQ( readback.row( 0 ).location, 0 );
    CHECK_EQ( readback.row( 0 ).size, 0 );
    CHECK_EQ( readback.row( 0 ).flags, 0 );
    CHECK( readback.classname( 0 ).empty() );
}

// ---------------------------------------------------------------------------
// serialize()/deserialize() — 4-row round trip incl. FENTTABLE_PLAYER,
// the id=0-skipped case, and an empty-classname row
// ---------------------------------------------------------------------------

static void test_roundtrip_four_rows()
{
    save::TokenTable  tokens( 64 );
    save::EntityTable table;
    table.init( 4 );

    // Row 0: id stays at its init() default of 0 (row index 0) — the
    // id=0-skipped case (D2 omits it) — but the row still carries REAL data
    // (location/classname), proving the skip is per-field, not per-row.
    table.row( 0 ).location = 500;
    table.set_classname( 0, "worldspawn" );

    // Row 1: FENTTABLE_PLAYER flag row — every field non-zero, so all 5
    // are written (id=1 from init() default is itself non-zero here).
    table.row( 1 ).location = 128;
    table.row( 1 ).size     = 64;
    table.row( 1 ).flags    = static_cast<int>( abi::k_fenttable_player );
    table.set_classname( 1, "player" );

    // Row 2: empty-classname row — id/location/size non-zero (written),
    // classname explicitly empty (omitted).
    table.row( 2 ).location = 192;
    table.row( 2 ).size     = 32;
    table.set_classname( 2, "" ); // empty classname must still round-trip

    // Row 3: an ordinary full-data row for baseline coverage.
    table.row( 3 ).location = 224;
    table.set_classname( 3, "func_door" );

    auto buf = save::create_save_buffer( g_pool, 1024, 64, 0.0f );
    REQUIRE( buf != nullptr );
    save::SaveBufferSink sink( *buf );

    auto w = table.serialize( sink, tokens );
    REQUIRE( w.has_value() );

    save::EntityTable readback;
    readback.init( 4 ); // MUST precede deserialize() — see entity_table.hpp doc
    std::size_t off  = 0;
    auto        data = buf->data();
    auto        r    = readback.deserialize( data, off, tokens );
    REQUIRE( r.has_value() );
    CHECK_EQ( off, data.size() );

    for ( std::size_t i = 0; i < 4; ++i )
    {
        CHECK_EQ( readback.row( i ).id, table.row( i ).id );
        CHECK_EQ( readback.row( i ).location, table.row( i ).location );
        CHECK_EQ( readback.row( i ).size, table.row( i ).size );
        CHECK_EQ( readback.row( i ).flags, table.row( i ).flags );
        CHECK( readback.classname( i ) == table.classname( i ) );
        CHECK( readback.row( i ).pent == nullptr ); // pent excluded from the wire format
    }

    // Row 0's id (0, skipped on the wire) survived via the pre-initialized
    // value, not a wire record.
    CHECK_EQ( readback.row( 0 ).id, 0 );

    // FENTTABLE_PLAYER survived the round trip on row 1.
    CHECK( ( static_cast<unsigned>( readback.row( 1 ).flags ) & abi::k_fenttable_player ) != 0 );
}

// ---------------------------------------------------------------------------
// Cross-check — a hand-built LEGACY-shape byte stream deserializes correctly
// (the "reads a GoldSrc-written table" witness)
// ---------------------------------------------------------------------------

static void test_cross_check_hand_built_legacy_stream()
{
    // Byte stream built purely by hand from the golden derivation rules
    // (D1/D2/D3) above — independent of EntityTable::serialize(). Proves
    // this repo's decoder accepts a stream it did not itself produce (e.g.
    // one written by an unmodified GoldSrc/HL-SDK codec).
    const std::array<std::byte, 23> legacy_stream = {
        // block header: { size=4, token(ETABLE)=1, payload=int32(actualCount=2) }
        b( 0x04 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        // id: { size=4, token(id)=2, payload=int32(1) }
        b( 0x04 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        // classname: { size=3, token(classname)=0, payload=text("ab")+NUL }
        b( 0x03 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x61 ), b( 0x62 ), b( 0x00 ),
    };

    // The token table a legacy writer would have produced: names interned
    // in write order (block name, then each written field's name), matching
    // the token indices baked into `legacy_stream` above.
    save::TokenTable tokens( 8 );
    auto etable_tok = tokens.insert( "ETABLE" );
    REQUIRE( etable_tok.has_value() );
    CHECK_EQ( *etable_tok, 1u );
    auto id_tok = tokens.insert( "id" );
    REQUIRE( id_tok.has_value() );
    CHECK_EQ( *id_tok, 2u );
    auto classname_tok = tokens.insert( "classname" );
    REQUIRE( classname_tok.has_value() );
    CHECK_EQ( *classname_tok, 0u );

    save::EntityTable t;
    t.init( 1 );
    std::size_t off = 0;
    auto        r   = t.deserialize( legacy_stream, off, tokens );
    REQUIRE( r.has_value() );
    CHECK_EQ( off, legacy_stream.size() );
    CHECK_EQ( t.row( 0 ).id, 1 );
    CHECK_EQ( t.row( 0 ).location, 0 );
    CHECK_EQ( t.row( 0 ).size, 0 );
    CHECK_EQ( t.row( 0 ).flags, 0 );
    CHECK( t.classname( 0 ) == "ab" );
    CHECK( t.row( 0 ).pent == nullptr );
}

// ---------------------------------------------------------------------------
// Corrupt input
// ---------------------------------------------------------------------------

static void test_corrupt_truncated()
{
    // Build one valid row (id=7, classname="x") to truncate. location/size/
    // flags stay at their init() zero -> omitted (D2), so the wire shape is
    // just: block header, id field, classname field (in that order).
    save::TokenTable  tokens( 16 );
    save::EntityTable table;
    table.init( 1 );
    table.row( 0 ).id = 7;
    table.set_classname( 0, "x" );

    auto buf = save::create_save_buffer( g_pool, 128, 16, 0.0f );
    REQUIRE( buf != nullptr );
    save::SaveBufferSink sink( *buf );
    REQUIRE( table.serialize( sink, tokens ).has_value() );

    auto full = buf->data();
    // block header (8) + id field (8) + classname field (4 header + 2
    // payload "x\0" = 6) = 22 bytes exactly.
    REQUIRE( full.size() == 22u );

    // Case 1: truncated mid-block-header (fewer than 4 bytes present at all
    // -> next_field_record's own header-length check fails first).
    {
        std::span<const std::byte> trunc = full.subspan( 0, 3 );
        save::EntityTable          t;
        t.init( 1 );
        std::size_t off = 0;
        auto        r   = t.deserialize( trunc, off, tokens );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::TruncatedBlock );
    }

    // Case 2: block header intact, but truncated mid-header of the SECOND
    // field record — classname, since location/size/flags are all skipped
    // for this row (block header 8 bytes + id field 8 bytes + 2 of
    // classname's 4 header bytes = 18 bytes).
    {
        std::span<const std::byte> trunc = full.subspan( 0, 18 );
        save::EntityTable          t;
        t.init( 1 );
        std::size_t off = 0;
        auto        r   = t.deserialize( trunc, off, tokens );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::TruncatedBlock );
    }
}

static void test_corrupt_block_name_mismatch()
{
    save::TokenTable tokens( 8 );

    auto buf = save::create_save_buffer( g_pool, 64, 8, 0.0f );
    REQUIRE( buf != nullptr );
    save::SaveBufferSink sink( *buf );

    // Write a block header naming "WRONGBLOCK" instead of "ETABLE".
    REQUIRE( save::write_block_header( sink, tokens, "WRONGBLOCK", 0 ).has_value() );

    save::EntityTable t;
    t.init( 1 );
    std::size_t off  = 0;
    auto        data = buf->data();
    auto        r    = t.deserialize( data, off, tokens );
    CHECK( !r.has_value() );
    CHECK( r.error() == save::SaveError::CorruptHeader );
}

// D1: classname's payload MUST end in a NUL — a payload that is otherwise
// well-formed (correct declared size, fully present) but missing its
// trailing terminator is BadFieldRecord, not silently accepted as a
// short/unterminated string.
static void test_corrupt_classname_missing_nul()
{
    save::TokenTable tokens( 8 );

    auto buf = save::create_save_buffer( g_pool, 64, 8, 0.0f );
    REQUIRE( buf != nullptr );
    save::SaveBufferSink sink( *buf );

    REQUIRE( save::write_block_header( sink, tokens, "ETABLE", 1 ).has_value() );

    // Hand-build a MALFORMED classname record: payload is "ab" WITHOUT the
    // required trailing NUL (contrast the golden's "ab\0").
    auto name_tok = tokens.insert( "classname" );
    REQUIRE( name_tok.has_value() );
    const std::array<std::byte, 2> bad_payload = { b( 0x61 ), b( 0x62 ) }; // "ab", no NUL
    REQUIRE( sink.write_field_record( *name_tok, bad_payload ).has_value() );

    save::EntityTable t;
    t.init( 1 );
    std::size_t off  = 0;
    auto        data = buf->data();
    auto        r    = t.deserialize( data, off, tokens );
    CHECK( !r.has_value() );
    CHECK( r.error() == save::SaveError::BadFieldRecord );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    g_pool = mem::create_pool( "entity_table_test" );

    RUN_TEST( test_init_parity );
    RUN_TEST( test_edict_from_clamp );
    RUN_TEST( test_row_golden );
    RUN_TEST( test_fully_zero_row );
    RUN_TEST( test_roundtrip_four_rows );
    RUN_TEST( test_cross_check_hand_built_legacy_stream );
    RUN_TEST( test_corrupt_truncated );
    RUN_TEST( test_corrupt_block_name_mismatch );
    RUN_TEST( test_corrupt_classname_missing_nul );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "entity_table: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
