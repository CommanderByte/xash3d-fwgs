// xash3dpp — delta struct-codec tests
// Legacy reference: engine/common/net_encode.c Test_RunDelta (ported with the
// same field specs, values, and epsilons) plus MSG_Write/ReadDeltaEntity,
// MSG_Write/ReadClientData, MSG_Write/ReadWeaponData, MSG_Write/
// ReadDeltaMovevars behaviours.

#include <xash3dpp/abi/entity_state.hpp>
#include <xash3dpp/abi/event_args.hpp>
#include <xash3dpp/abi/pm_movevars.hpp>
#include <xash3dpp/abi/usercmd.hpp>
#include <xash3dpp/abi/weaponinfo.hpp>
#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>
#include <xash3dpp/private/networking/delta/wire_format.hpp>

#include "../../test_helpers.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

using namespace xash::networking;
using namespace xash::networking::delta;

static int g_pass = 0, g_fail = 0;

namespace
{

[[nodiscard]] bool equal_eps( float a, float b, float e )
{
    return ( a >= b - e ) && ( a <= b + e );
}

} // namespace

// -- port of legacy Test_RunDelta (XASH_ENGINE_TESTS) --------------------------
// Same struct shape, field specs, input values, and assertion epsilons.

namespace
{

struct delta_test_struct_t
{
    char          dt_string[128];
    float         dt_timewindow_big;
    float         dt_timewindow_8;
    float         dt_angle;
    float         dt_float_signed;
    float         dt_float_unsigned;
    std::int32_t  dt_integer_signed;
    std::uint32_t dt_integer_unsigned;
    std::int16_t  dt_short_signed;
    std::uint16_t dt_short_unsigned;
    std::int8_t   dt_byte_signed;
    std::uint8_t  dt_byte_unsigned;
};

#define TEST_FIELD( member_, flags_, bits_, mult_, post_ )                        \
    DeltaField{ #member_,                                                         \
                static_cast<int>( offsetof( delta_test_struct_t, member_ )),      \
                static_cast<int>( sizeof( delta_test_struct_t{}.member_ )),       \
                ( flags_ ), ( mult_ ), ( post_ ), ( bits_ ), false }

const std::array<DeltaField, 12> k_test_fields =
{
    TEST_FIELD( dt_string,           k_dt_string,                   1, 1.0f,     1.0f ),
    TEST_FIELD( dt_timewindow_big,   k_dt_timewindow_big,          24, 1000.0f,  1.0f ),
    TEST_FIELD( dt_timewindow_8,     k_dt_timewindow_8,             8, 1.0f,     1.0f ),
    TEST_FIELD( dt_angle,            k_dt_angle,                   16, 1.0f,     1.0f ),
    TEST_FIELD( dt_float_signed,     k_dt_float | k_dt_signed,     22, 100.0f,   1.0f ),
    TEST_FIELD( dt_float_unsigned,   k_dt_float,                   24, 10000.0f, 0.1f ),
    TEST_FIELD( dt_integer_signed,   k_dt_integer | k_dt_signed,   24, 1.0f,     1.0f ),
    TEST_FIELD( dt_integer_unsigned, k_dt_integer,                 24, 1.0f,     1.0f ),
    TEST_FIELD( dt_short_signed,     k_dt_short | k_dt_signed,     16, 1.0f,     1.0f ),
    TEST_FIELD( dt_short_unsigned,   k_dt_short,                   15, 0.125f,   1.0f ),
    TEST_FIELD( dt_byte_signed,      k_dt_byte | k_dt_signed,       6, 1.0f,     1.0f ),
    TEST_FIELD( dt_byte_unsigned,    k_dt_byte,                     8, 1.0f,     1.0f ),
};

#undef TEST_FIELD

} // namespace

static void test_run_delta_port()
{
    const double timebase = 123.123;

    delta_test_struct_t null_state {};
    delta_test_struct_t src {};
    delta_test_struct_t decoded {};

    std::memcpy( src.dt_string, "test data check it's the same", 30 );
    src.dt_timewindow_big   = static_cast<float>( timebase + 2.3456 );
    src.dt_timewindow_8     = static_cast<float>( timebase + 0.0234 );
    src.dt_angle            = 160.245f;
    src.dt_float_signed     = -15.123f;
    src.dt_float_unsigned   = 1235.321f;
    src.dt_integer_signed   = -412784;
    src.dt_integer_unsigned = 123453;
    src.dt_short_signed     = -12343;
    src.dt_short_unsigned   = 32131;
    src.dt_byte_signed      = 16;
    src.dt_byte_unsigned    = 218;

    std::array<std::byte, 4096> buf{};
    MessageBuf msg{ buf };

    (void)xash_delta_wire_format().write_fields(
        msg, k_test_fields, &null_state, &src, timebase );

    msg.reset();
    xash_delta_wire_format().read_fields(
        msg, k_test_fields, &null_state, &decoded, timebase );

    CHECK_STREQ( src.dt_string, decoded.dt_string );

    // epsilons derived from multipliers, exactly as the legacy test
    CHECK( equal_eps( src.dt_timewindow_big, decoded.dt_timewindow_big, 0.001f ));
    CHECK( equal_eps( src.dt_timewindow_8, decoded.dt_timewindow_8, 0.01f ));
    CHECK( equal_eps( src.dt_angle, decoded.dt_angle, 0.1f ));
    CHECK( equal_eps( src.dt_float_signed, decoded.dt_float_signed, 0.01f ));

    // post-multiplier does not affect network data; revert before comparing
    CHECK( equal_eps( src.dt_float_unsigned, decoded.dt_float_unsigned * 10.f, 0.01f ));

    CHECK_EQ( src.dt_integer_signed, decoded.dt_integer_signed );
    CHECK_EQ( src.dt_integer_unsigned, decoded.dt_integer_unsigned );
    CHECK_EQ( src.dt_short_signed, decoded.dt_short_signed );
    CHECK_EQ( src.dt_short_unsigned & ( 0xffff << 3 ), decoded.dt_short_unsigned );
    CHECK_EQ( src.dt_byte_signed, decoded.dt_byte_signed );
    CHECK_EQ( src.dt_byte_unsigned, decoded.dt_byte_unsigned );
}

// -- usercmd round trip incl. viewangles normalisation --------------------------

namespace
{

const char *k_codec_script =
    "usercmd_t none\n"
    "{\n"
    "    DEFINE_DELTA( msec, DT_BYTE, 8, 1.0 ),\n"
    "    DEFINE_DELTA( buttons, DT_SHORT, 16, 1.0 ),\n"
    "    DEFINE_DELTA( forwardmove, DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
    "    DEFINE_DELTA( viewangles[1], DT_ANGLE, 16, 1.0 )\n"
    "}\n"
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 ),\n"
    "    DEFINE_DELTA( ducking, DT_INTEGER, 1, 1.0 ),\n"
    "    DEFINE_DELTA( iparam1, DT_INTEGER | DT_SIGNED, 18, 1.0 )\n"
    "}\n"
    "entity_state_t none\n"
    "{\n"
    "    DEFINE_DELTA( origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
    "    DEFINE_DELTA( sequence, DT_INTEGER, 8, 1.0 ),\n"
    "    DEFINE_DELTA( rendercolor.r, DT_BYTE, 8, 1.0 )\n"
    "}\n"
    "clientdata_t none\n"
    "{\n"
    "    DEFINE_DELTA( health, DT_SIGNED | DT_FLOAT, 16, 1.0 ),\n"
    "    DEFINE_DELTA( waterlevel, DT_INTEGER, 2, 1.0 )\n"
    "}\n"
    "weapon_data_t none\n"
    "{\n"
    "    DEFINE_DELTA( m_iClip, DT_SIGNED | DT_INTEGER, 10, 1.0 )\n"
    "}\n";

} // namespace

static void test_usercmd_round_trip()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::usercmd_t from {}, to {}, decoded {};
    to.msec           = 33;
    to.buttons        = 0x0403;
    to.forwardmove    = 250.0f;
    to.viewangles[1]  = 270.0f; // bit-angle decodes to -90 after wrap

    std::array<std::byte, 256> buf{};
    MessageBuf msg{ buf };
    tables.write_delta_usercmd( msg, &from, &to );

    msg.reset();
    tables.read_delta_usercmd( msg, &from, &decoded );

    CHECK_EQ( decoded.msec, 33 );
    CHECK_EQ( decoded.buttons, 0x0403u );
    CHECK( decoded.forwardmove == 250.0f );
    CHECK( decoded.viewangles[1] == -90.0f ); // normalised like COM_NormalizeAngles
    CHECK_EQ( tables.stats().structs_encoded.load(), 1u );
    CHECK_EQ( tables.stats().structs_decoded.load(), 1u );
}

// -- entity codec ------------------------------------------------------------------

static void test_entity_round_trip()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::entity_state_t from {}, to {}, decoded {};
    to.number        = 42;
    to.entityType    = ::xash::abi::k_entity_normal;
    to.origin[0]     = -100.5f;
    to.sequence      = 7;
    to.rendercolor.r = 200;

    WriteDeltaEntityParams wp;
    wp.force      = true; // baseline-style full update
    wp.max_edicts = 8192;

    std::array<std::byte, 512> buf{};
    MessageBuf msg{ buf };
    REQUIRE( tables.write_delta_entity( msg, &from, &to, wp ));

    msg.reset();
    const int number = static_cast<int>( msg.read_ubit_long( 13 ));
    CHECK_EQ( number, 42 );

    ReadDeltaEntityParams rp;
    rp.number       = number;
    rp.max_entities = 4096;

    REQUIRE( tables.read_delta_entity( msg, &from, &decoded, rp ));
    CHECK_EQ( decoded.number, 42 );
    CHECK_EQ( decoded.entityType, ::xash::abi::k_entity_normal );
    CHECK( decoded.origin[0] == -100.5f );
    CHECK_EQ( decoded.sequence, 7 );
    CHECK_EQ( decoded.rendercolor.r, 200u );
}

static void test_entity_remove_messages()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::entity_state_t from {};
    from.number = 55;

    // removeType 1 — leave PVS
    {
        std::array<std::byte, 64> buf{};
        MessageBuf msg{ buf };
        WriteDeltaEntityParams wp; // force=false
        REQUIRE( tables.write_delta_entity( msg, &from, nullptr, wp ));
        CHECK_EQ( msg.num_bits_written(), 13u + 2u );

        msg.reset();
        ::xash::abi::entity_state_t decoded {};
        decoded.sequence = 99; // must be zeroed by the remove path
        ReadDeltaEntityParams rp;
        rp.number = static_cast<int>( msg.read_ubit_long( 13 ));
        CHECK( !tables.read_delta_entity( msg, &from, &decoded, rp ));
        CHECK_EQ( decoded.sequence, 0 );
        CHECK_EQ( decoded.number, 0 );
    }

    // removeType 2 — full server remove
    {
        std::array<std::byte, 64> buf{};
        MessageBuf msg{ buf };
        WriteDeltaEntityParams wp;
        wp.force = true;
        REQUIRE( tables.write_delta_entity( msg, &from, nullptr, wp ));

        msg.reset();
        ::xash::abi::entity_state_t decoded {};
        ReadDeltaEntityParams rp;
        rp.number = static_cast<int>( msg.read_ubit_long( 13 ));
        CHECK( !tables.read_delta_entity( msg, &from, &decoded, rp ));
        CHECK_EQ( decoded.number, -1 );
    }
}

static void test_entity_rollback_and_bad_number()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::entity_state_t state {};
    state.number     = 3;
    state.entityType = ::xash::abi::k_entity_normal;

    std::array<std::byte, 128> buf{};
    MessageBuf msg{ buf };

    // identical from/to without force: whole message rolled back
    WriteDeltaEntityParams wp;
    wp.max_edicts = 8192;
    REQUIRE( tables.write_delta_entity( msg, &state, &state, wp ));
    CHECK_EQ( msg.num_bits_written(), 0u );

    // bad entity number refuses to write (legacy Host_Error)
    ::xash::abi::entity_state_t bad = state;
    bad.number = 9000;
    CHECK( !tables.write_delta_entity( msg, &state, &bad, wp ));
    CHECK_EQ( msg.num_bits_written(), 0u );
}

namespace
{

struct StubBaselines final : IBaselineResolver
{
    const ::xash::abi::entity_state_t *state { nullptr };
    std::int32_t    seen_offset { 0 };
    DeltaEntityKind seen_kind   { DeltaEntityKind::Entity };

    const ::xash::abi::entity_state_t *
    resolve( std::int32_t baseline_offset, DeltaEntityKind kind ) noexcept override
    {
        seen_offset = baseline_offset;
        seen_kind   = kind;
        return state;
    }
};

} // namespace

static void test_entity_baseline_resolution()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::entity_state_t baseline {};
    baseline.entityType = ::xash::abi::k_entity_normal;
    baseline.sequence   = 42;

    ::xash::abi::entity_state_t from {}, to = baseline;
    from.entityType = ::xash::abi::k_entity_normal;
    to.number = 5;

    WriteDeltaEntityParams wp;
    wp.baseline   = -3;
    wp.max_edicts = 8192;

    std::array<std::byte, 256> buf{};
    MessageBuf msg{ buf };
    // write against the same baseline the reader will resolve
    REQUIRE( tables.write_delta_entity( msg, &baseline, &to, wp ));

    msg.reset();
    StubBaselines stub;
    stub.state = &baseline;

    ::xash::abi::entity_state_t decoded {};
    ReadDeltaEntityParams rp;
    rp.number    = static_cast<int>( msg.read_ubit_long( 13 ));
    rp.baselines = &stub;

    REQUIRE( tables.read_delta_entity( msg, &from, &decoded, rp ));
    CHECK_EQ( stub.seen_offset, -3 );
    CHECK_EQ( decoded.sequence, 42 ); // came from the resolved baseline
    CHECK_EQ( decoded.number, 5 );
}

// -- clientdata --------------------------------------------------------------------

static void test_clientdata_round_trip_and_rollback()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::clientdata_t from {}, to {}, decoded {};
    to.health     = 88.0f;
    to.waterlevel = 2;

    {
        std::array<std::byte, 256> buf{};
        MessageBuf msg{ buf };
        tables.write_clientdata( msg, &from, &to, 0.0 );
        CHECK( msg.num_bits_written() > 1u );

        msg.reset();
        tables.read_clientdata( msg, &from, &decoded, 0.0 );
        CHECK( decoded.health == 88.0f );
        CHECK_EQ( decoded.waterlevel, 2 );
    }

    // no changes: message collapses to a single 0 bit
    {
        std::array<std::byte, 256> buf{};
        MessageBuf msg{ buf };
        tables.write_clientdata( msg, &to, &to, 0.0 );
        CHECK_EQ( msg.num_bits_written(), 1u );

        msg.reset();
        ::xash::abi::clientdata_t copied {};
        tables.read_clientdata( msg, &to, &copied, 0.0 );
        CHECK( copied.health == 88.0f ); // copied from `from`
        CHECK_EQ( copied.waterlevel, 2 );
    }
}

// -- weapon data ------------------------------------------------------------------

static void test_weapon_data_round_trip_and_rollback()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::weapon_data_t from {}, to {}, decoded {};
    to.m_iClip = -1;

    {
        std::array<std::byte, 128> buf{};
        MessageBuf msg{ buf };
        tables.write_weapon_data( msg, &from, &to, 0.0, 17 );

        msg.reset();
        CHECK_EQ( msg.read_one_bit(), 1 );
        CHECK_EQ( msg.read_ubit_long( 6 ), 17u );
        tables.read_weapon_data( msg, &from, &decoded, 0.0 );
        CHECK_EQ( decoded.m_iClip, -1 );
    }

    // no changes: everything rolled back, nothing on the wire
    {
        std::array<std::byte, 128> buf{};
        MessageBuf msg{ buf };
        tables.write_weapon_data( msg, &to, &to, 0.0, 17 );
        CHECK_EQ( msg.num_bits_written(), 0u );
    }
}

// -- movevars ---------------------------------------------------------------------

static void test_movevars_command_and_rollback()
{
    DeltaTables tables;
    // no movevars_t section -> built-in fallback table (27 fields)
    REQUIRE( tables.init_from_script(
        "event_t none\n{\n    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n}\n" ));

    ::xash::abi::movevars_t from {}, to {}, decoded {};
    to.gravity  = 800.0f;
    to.maxspeed = 320.0f;

    {
        std::array<std::byte, 512> buf{};
        MessageBuf msg{ buf };
        REQUIRE( tables.write_delta_movevars( msg, &from, &to, 0x2C ));
        CHECK_EQ( std::to_integer<std::uint8_t>( msg.data()[0] ), 0x2Cu );

        msg.reset();
        CHECK_EQ( msg.read_byte(), 0x2Cu ); // dispatcher consumes the command
        tables.read_delta_movevars( msg, &from, &decoded );
        CHECK( decoded.gravity == 800.0f );
        CHECK( decoded.maxspeed == 320.0f );
    }

    // identical states: message killed entirely, command byte included
    {
        std::array<std::byte, 512> buf{};
        MessageBuf msg{ buf };
        CHECK( !tables.write_delta_movevars( msg, &to, &to, 0x2C ));
        CHECK_EQ( msg.num_bits_written(), 0u );
    }
}

// -- GoldSrc batch codec through real tables ------------------------------------

static void test_gs_fields_round_trip()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::usercmd_t from {}, to {}, decoded {};
    to.msec        = 12;
    to.forwardmove = -125.0f; // exercises GS sign-magnitude float path

    std::array<std::byte, 256> buf{};
    MessageBuf msg{ buf };
    tables.write_gs_fields( msg, DeltaStructId::Usercmd, &from, &to, 0.0 );

    msg.reset();
    tables.read_gs_fields( msg, DeltaStructId::Usercmd, &from, &decoded, 0.0 );
    CHECK_EQ( decoded.msec, 12 );
    CHECK( decoded.forwardmove == -125.0f );
}

// -- custom-encode masking end-to-end -----------------------------------------------

namespace
{

DeltaTables *g_active_tables = nullptr;

void masking_encoder( DeltaField *fields,
                      const std::uint8_t * /*from*/, const std::uint8_t * /*to*/ )
{
    if( g_active_tables )
        g_active_tables->unset_field( fields, "ducking" );
}

} // namespace

static void test_custom_encode_masks_field()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));
    REQUIRE( tables.register_encoder( "Game_EventEncode", &masking_encoder ));
    g_active_tables = &tables;

    ::xash::abi::event_args_t from {}, to {}, decoded {};
    to.entindex = 100;
    to.ducking  = 1; // changed, but the encoder suppresses it

    std::array<std::byte, 128> buf{};
    MessageBuf msg{ buf };
    tables.write_delta_event( msg, &from, &to );

    msg.reset();
    tables.read_delta_event( msg, &from, &decoded );
    CHECK_EQ( decoded.entindex, 100 );
    CHECK_EQ( decoded.ducking, 0 ); // masked out on the wire

    g_active_tables = nullptr;
}

// -- test_baseline bit counts ---------------------------------------------------------

static void test_baseline_bit_counts()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_codec_script ));

    ::xash::abi::entity_state_t a {}, b {};
    a.entityType = b.entityType = ::xash::abi::k_entity_normal;

    // null cases
    CHECK_EQ( tables.test_baseline( nullptr, nullptr, false, 0.0 ), 0 );
    CHECK_EQ( tables.test_baseline( &a, nullptr, false, 0.0 ), 13 + 2 );

    // identical: header (13+2) + entityType flag (1) + 3 per-field flags
    CHECK_EQ( tables.test_baseline( &a, &a, false, 0.0 ), 13 + 2 + 1 + 3 );

    // one changed 8-bit integer field adds its payload width
    b.sequence = 5;
    CHECK_EQ( tables.test_baseline( &a, &b, false, 0.0 ), 13 + 2 + 1 + 3 + 8 );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_run_delta_port );
    RUN_TEST( test_usercmd_round_trip );
    RUN_TEST( test_entity_round_trip );
    RUN_TEST( test_entity_remove_messages );
    RUN_TEST( test_entity_rollback_and_bad_number );
    RUN_TEST( test_entity_baseline_resolution );
    RUN_TEST( test_clientdata_round_trip_and_rollback );
    RUN_TEST( test_weapon_data_round_trip_and_rollback );
    RUN_TEST( test_movevars_command_and_rollback );
    RUN_TEST( test_gs_fields_round_trip );
    RUN_TEST( test_custom_encode_masks_field );
    RUN_TEST( test_baseline_bit_counts );

    std::printf( "delta_codec: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
