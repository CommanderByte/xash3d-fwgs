// xash3dpp — delta table-descriptor wire tests
// Legacy reference: engine/common/net_encode.c Delta_WriteTableField /
// Delta_WriteDescriptionToClient / Delta_ParseTableField /
// Delta_ParseTableField_GS.
//
// Golden derivation (LSB-first): descriptor after the command byte is
// tableIndex(4) nameIndex(8) flags(10) bits-1(5) mulbit(1) [float] postbit(1).

#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>
#include <xash3dpp/private/networking/delta/field_defs.hpp>
#include <xash3dpp/private/networking/delta/wire_format.hpp>

#include "../../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstring>

using namespace xash::networking;
using namespace xash::networking::delta;

static int g_pass = 0, g_fail = 0;

namespace
{

constexpr std::uint32_t k_svc_deltatable = 0x3C; // arbitrary test command value

[[nodiscard]] std::uint8_t byte_at( const MessageBuf &msg, std::size_t i )
{
    return std::to_integer<std::uint8_t>( msg.data()[ i ] );
}

const DeltaField *field_by_name( DeltaTables &tables, DeltaStructId id,
                                 const char *name )
{
    const DeltaField *fields = tables.table_fields( id );
    if( !fields )
        return nullptr;
    for( int i = 0; i < tables.table_field_count( id ); ++i )
        if( std::strcmp( fields[ i ].name, name ) == 0 )
            return &fields[ i ];
    return nullptr;
}

} // namespace

// -- single descriptor golden --------------------------------------------------
// event_t entindex: tableIndex=0 (Event), nameIndex=1 (entindex is ev_fields[1]),
// flags=DT_INTEGER (0x008), bits-1=10, mul==1.0 -> 0 bit, post==1.0 -> 0 bit.
//   byte0 = 0x3C (command)
//   byte1 (bits 8-15):  tblIdx 0000, nameIdx low4 = 1,0,0,0     -> 0x10
//   byte2 (bits 16-23): nameIdx high4 = 0, flags low4 = 0,0,0,1 -> 0x80
//   byte3 (bits 24-31): flags high6 = 0, (bits-1)=10 low2 = 0,1 -> 0x80
//   byte4 (bits 32-36): (bits-1) high3 = 0,1,0 ; mul 0 ; post 0 -> 0x02
//   total 8 + 29 = 37 bits

static void test_descriptor_golden()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script(
        "event_t none\n{\n    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n}\n" ));

    // suppress the movevars fallback noise: write only via a fresh script
    // with just the one section is not possible (fallback always applies),
    // so count the expected messages instead: 1 event field + 27 movevars.
    std::array<std::byte, 2048> buf{};
    MessageBuf msg{ buf };
    tables.write_description( msg, k_svc_deltatable );

    // The first descriptor written is the event_t entindex field.
    CHECK_EQ( byte_at( msg, 0 ), 0x3Cu );
    CHECK_EQ( byte_at( msg, 1 ), 0x10u );
    CHECK_EQ( byte_at( msg, 2 ), 0x80u );
    CHECK_EQ( byte_at( msg, 3 ), 0x80u );
    CHECK_EQ( byte_at( msg, 4 ) & 0x1Fu, 0x02u ); // low 5 bits of byte4
}

// -- full description round-trip (server write -> client parse) -----------------

static void test_description_round_trip()
{
    static const char *script =
        "event_t none\n"
        "{\n"
        "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 ),\n"
        "    DEFINE_DELTA( origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
        "    DEFINE_DELTA_POST( fparam1, DT_SIGNED | DT_FLOAT, 20, 100.0, 0.1 )\n"
        "}\n";

    DeltaTables server;
    REQUIRE( server.init_from_script( script ));

    std::array<std::byte, 4096> buf{};
    MessageBuf msg{ buf };
    server.write_description( msg, k_svc_deltatable );

    const std::size_t total_bits = msg.num_bits_written();

    // client side: dispatch loop — command byte then one descriptor each
    DeltaTables client;
    msg.reset();
    while( msg.tell_bit() < total_bits )
    {
        CHECK_EQ( msg.read_byte(), 0x3Cu );
        REQUIRE( client.parse_table_field( msg ));
    }
    client.init_client();

    CHECK( client.is_initialized());
    CHECK( client.table_initialized( DeltaStructId::Event ));
    CHECK_EQ( client.table_field_count( DeltaStructId::Event ), 3 );
    CHECK_EQ( client.table_field_count( DeltaStructId::Movevars ), 27 );

    const DeltaField *origin0 = field_by_name( client, DeltaStructId::Event, "origin[0]" );
    REQUIRE( origin0 != nullptr );
    CHECK_EQ( origin0->flags, k_dt_signed | k_dt_float );
    CHECK_EQ( origin0->bits, 16 );
    CHECK( origin0->multiplier == 8.0f );
    CHECK_EQ( origin0->offset, 8 ); // restored from the client's own field info

    const DeltaField *fparam1 = field_by_name( client, DeltaStructId::Event, "fparam1" );
    REQUIRE( fparam1 != nullptr );
    CHECK( fparam1->multiplier == 100.0f );
    CHECK( fparam1->post_multiplier == 0.1f );
}

// -- malformed descriptors -------------------------------------------------------

static void test_parse_bad_table_index()
{
    std::array<std::byte, 16> buf{};
    MessageBuf msg{ buf };
    msg.write_ubit_long( 15, 4 ); // only 8 tables exist
    msg.write_ubit_long( 0, 8 );
    msg.write_ubit_long( 8, 10 );
    msg.write_ubit_long( 10, 5 );
    msg.write_one_bit( 0 );
    msg.write_one_bit( 0 );
    msg.reset();

    DeltaTables tables;
    CHECK( !tables.parse_table_field( msg ));
}

static void test_parse_bad_name_index_ignored()
{
    std::array<std::byte, 16> buf{};
    MessageBuf msg{ buf };
    msg.write_ubit_long( 0, 4 );   // event_t
    msg.write_ubit_long( 200, 8 ); // ev_fields has 18 entries
    msg.write_ubit_long( 8, 10 );
    msg.write_ubit_long( 10, 5 );
    msg.write_one_bit( 0 );
    msg.write_one_bit( 0 );
    msg.reset();

    DeltaTables tables;
    CHECK( tables.parse_table_field( msg )); // consumed fine, field ignored
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 0 );
}

// -- local-game wipe quirk ---------------------------------------------------------
// A live (initialised) table set receiving its first wire descriptor is wiped
// completely before the field is applied (legacy Delta_Shutdown call).

static void test_local_game_wipe_quirk()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script(
        "event_t none\n{\n    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n}\n" ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Movevars ), 27 );

    std::array<std::byte, 16> buf{};
    MessageBuf msg{ buf };
    msg.write_ubit_long( 0, 4 ); // event_t
    msg.write_ubit_long( 1, 8 ); // entindex
    msg.write_ubit_long( 8, 10 );
    msg.write_ubit_long( 10, 5 );
    msg.write_one_bit( 0 );
    msg.write_one_bit( 0 );
    msg.reset();

    REQUIRE( tables.parse_table_field( msg ));

    CHECK( !tables.is_initialized());                                // wiped
    CHECK_EQ( tables.table_field_count( DeltaStructId::Movevars ), 0 ); // fallback gone
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 1 );    // re-added
}

// -- GoldSrc description parse -------------------------------------------------------

static void test_parse_table_gs_round_trip()
{
    // Hand-build one GS table description: usercmd_t with two fields, one
    // carrying DT_SIGNED_GS that must remap to DT_SIGNED.
    std::array<std::byte, 512> buf{};
    MessageBuf msg{ buf };

    REQUIRE( msg.write_string( "usercmd_t" ));
    msg.write_short( 2 );

    const goldsrc_delta_t null_desc {};

    goldsrc_delta_t d1 {};
    d1.fieldType = static_cast<int>( k_dt_byte );
    std::memcpy( d1.fieldName, "msec", 5 );
    d1.fieldOffset      = 2;
    d1.fieldSize        = 1;
    d1.significant_bits = 8;
    d1.premultiply      = 1.0f;
    d1.postmultiply     = 1.0f;
    goldsrc_delta_wire_format().write_fields(
        msg, k_goldsrc_meta_runtime, &null_desc, &d1, 0.0 );

    goldsrc_delta_t d2 {};
    d2.fieldType = static_cast<int>( k_dt_float | k_dt_signed_gs );
    std::memcpy( d2.fieldName, "forwardmove", 12 );
    d2.fieldOffset      = 16;
    d2.fieldSize        = 4;
    d2.significant_bits = 12;
    d2.premultiply      = 0.25f; // survives the x4000 meta float scaling
    d2.postmultiply     = 1.0f;
    goldsrc_delta_wire_format().write_fields(
        msg, k_goldsrc_meta_runtime, &null_desc, &d2, 0.0 );

    msg.reset();

    DeltaTables tables;
    REQUIRE( tables.parse_table_gs( msg ));
    CHECK_EQ( msg.tell_bit() & 7u, 0u ); // byte-aligned like MSG_EndBitWriting

    tables.init_client();
    CHECK( tables.is_initialized());
    CHECK_EQ( tables.table_field_count( DeltaStructId::Usercmd ), 2 );

    const DeltaField *msec = field_by_name( tables, DeltaStructId::Usercmd, "msec" );
    REQUIRE( msec != nullptr );
    CHECK_EQ( msec->flags, k_dt_byte );
    CHECK_EQ( msec->bits, 8 );
    CHECK_EQ( msec->offset, 2 ); // from the local info table, not the wire

    const DeltaField *fwd = field_by_name( tables, DeltaStructId::Usercmd, "forwardmove" );
    REQUIRE( fwd != nullptr );
    CHECK_EQ( fwd->flags, k_dt_float | k_dt_signed ); // GS sign bit remapped
    CHECK_EQ( fwd->bits, 12 );
    CHECK( fwd->multiplier == 0.25f );
}

static void test_parse_table_gs_unknown_struct()
{
    std::array<std::byte, 64> buf{};
    MessageBuf msg{ buf };
    REQUIRE( msg.write_string( "bogus_t" ));
    msg.write_short( 0 );
    msg.reset();

    DeltaTables tables;
    CHECK( !tables.parse_table_gs( msg ));
}

static void test_parse_table_gs_too_many_fields()
{
    std::array<std::byte, 64> buf{};
    MessageBuf msg{ buf };
    REQUIRE( msg.write_string( "usercmd_t" ));
    msg.write_short( 100 ); // cmd_fields has 16 entries
    msg.reset();

    DeltaTables tables;
    CHECK( !tables.parse_table_gs( msg ));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_descriptor_golden );
    RUN_TEST( test_description_round_trip );
    RUN_TEST( test_parse_bad_table_index );
    RUN_TEST( test_parse_bad_name_index_ignored );
    RUN_TEST( test_local_game_wipe_quirk );
    RUN_TEST( test_parse_table_gs_round_trip );
    RUN_TEST( test_parse_table_gs_unknown_struct );
    RUN_TEST( test_parse_table_gs_too_many_fields );

    std::printf( "delta_table_wire: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
