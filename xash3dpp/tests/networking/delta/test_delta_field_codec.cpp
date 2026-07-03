// xash3dpp — delta field codec golden-vector tests
// Legacy reference: engine/common/net_encode.c (Delta_WriteField_/ReadField_/
// CompareField/ClampIntegerField).
//
// Every golden vector documents its bit math.  MessageBuf packs LSB-first:
// the first written bit lands in bit 0 of byte 0.  write_sbit_long stores the
// two's-complement value truncated to N bits; read_sbit_long sign-extends
// from bit N-1.

#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>
#include <xash3dpp/private/networking/delta/field_codec.hpp>

#include "../../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace xash::networking;
using namespace xash::networking::delta;

static int g_pass = 0, g_fail = 0;

namespace
{

// Mirror of the legacy XASH_ENGINE_TESTS delta_test_struct_t idea: one field
// of every codec type at known offsets.
struct TestState
{
    std::uint8_t  u8      { 0 };
    std::int8_t   s8      { 0 };
    std::uint16_t u16     { 0 };
    std::int16_t  s16     { 0 };
    std::uint32_t u32     { 0 };
    std::int32_t  s32     { 0 };
    float         f       { 0.0f };
    float         ang     { 0.0f };
    float         tw8     { 0.0f };
    float         twbig   { 0.0f };
    char          str[32] {};
};

DeltaField make_field( const char *name, int offset, int size,
                       std::uint32_t flags, float mult, float post, int bits )
{
    DeltaField f;
    f.name            = name;
    f.offset          = offset;
    f.size            = size;
    f.flags           = flags;
    f.multiplier      = mult;
    f.post_multiplier = post;
    f.bits            = bits;
    return f;
}

#define FIELD_OF( member_, flags_, mult_, post_, bits_ )                     \
    make_field( #member_, static_cast<int>( offsetof( TestState, member_ )), \
                static_cast<int>( sizeof( TestState{}.member_ )),            \
                ( flags_ ), ( mult_ ), ( post_ ), ( bits_ ))

[[nodiscard]] std::uint8_t byte_at( const MessageBuf &msg, std::size_t i )
{
    return std::to_integer<std::uint8_t>( msg.data()[ i ] );
}

} // namespace

// -- clamp_integer_field ----------------------------------------------------

static void test_clamp_integer_field()
{
    // unsigned 4 bits: maxnum = 2^4 - 1 = 15
    CHECK_EQ( clamp_integer_field( 100, 0, 4 ), 15 );
    // signed 4 bits: signbits = 3, range [-8, 7]
    CHECK_EQ( clamp_integer_field( 100, 1, 4 ), 7 );
    CHECK_EQ( clamp_integer_field( -100, 1, 4 ), -8 );
    // 32 bits: pass-through
    CHECK_EQ( clamp_integer_field( -100, 1, 32 ), -100 );
    CHECK_EQ( clamp_integer_field( 12345, 0, 32 ), 12345 );
}

// -- DT_BYTE unsigned, bits=8 ------------------------------------------------
// value 100 = 0b0110'0100 -> ubit(100,8) -> byte0 = 0x64

static void test_byte_unsigned_golden()
{
    const DeltaField fld = FIELD_OF( u8, k_dt_byte, 1.0f, 1.0f, 8 );
    TestState from{}, to{};
    to.u8 = 100;

    std::array<std::byte, 64> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, 0.0 );

    CHECK_EQ( msg.num_bits_written(), 8u );
    CHECK_EQ( byte_at( msg, 0 ), 0x64u );

    TestState decoded{};
    msg.reset();
    read_field_payload( msg, fld, &decoded, 0.0 );
    CHECK_EQ( decoded.u8, 100u );

    CHECK( !compare_field( fld, &from, &to ));
    CHECK( compare_field( fld, &to, &to ));
}

// -- DT_SHORT | DT_SIGNED, bits=16 -------------------------------------------
// -12343 = 0xFFFFCFC9; & 0xFFFF = 0xCFC9 -> LSB-first bytes C9 CF

static void test_short_signed_golden()
{
    const DeltaField fld = FIELD_OF( s16, k_dt_short | k_dt_signed, 1.0f, 1.0f, 16 );
    TestState to{};
    to.s16 = -12343;

    std::array<std::byte, 64> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, 0.0 );

    CHECK_EQ( msg.num_bits_written(), 16u );
    CHECK_EQ( byte_at( msg, 0 ), 0xC9u );
    CHECK_EQ( byte_at( msg, 1 ), 0xCFu );

    TestState decoded{};
    msg.reset();
    read_field_payload( msg, fld, &decoded, 0.0 );
    CHECK_EQ( decoded.s16, -12343 );
}

// -- DT_INTEGER with multiplier 0.125, bits=15 --------------------------------
// write: 1000 * 0.125 = 125.0f -> uint 125 -> ubit(125,15): 125 = 0x7D
//   byte0 = 0x7D, byte1 low7 = 0 -> bytes 7D 00
// read: 125 / 0.125 = 1000.0f -> uint 1000 (integer-domain scaling round-trip)

static void test_integer_multiplier_chain()
{
    const DeltaField fld = FIELD_OF( u32, k_dt_integer, 0.125f, 1.0f, 15 );
    TestState to{};
    to.u32 = 1000;

    std::array<std::byte, 64> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, 0.0 );

    CHECK_EQ( msg.num_bits_written(), 15u );
    CHECK_EQ( byte_at( msg, 0 ), 0x7Du );
    CHECK_EQ( byte_at( msg, 1 ), 0x00u );

    TestState decoded{};
    msg.reset();
    read_field_payload( msg, fld, &decoded, 0.0 );
    CHECK_EQ( decoded.u32, 1000u );
}

// -- clamp saturation through the write path ---------------------------------
// unsigned byte bits=4, value 100 -> clamped 15 -> ubit(15,4) -> byte0 = 0x0F
// signed byte bits=4, value -100 -> clamped -8 -> sbit(-8,4) = 0x8 -> byte0 = 0x08

static void test_write_clamps()
{
    {
        const DeltaField fld = FIELD_OF( u8, k_dt_byte, 1.0f, 1.0f, 4 );
        TestState to{};
        to.u8 = 100;

        std::array<std::byte, 8> buf{};
        MessageBuf msg{ buf };
        write_field_payload( msg, fld, &to, 0.0 );
        CHECK_EQ( msg.num_bits_written(), 4u );
        CHECK_EQ( byte_at( msg, 0 ), 0x0Fu );
    }
    {
        const DeltaField fld = FIELD_OF( s8, k_dt_byte | k_dt_signed, 1.0f, 1.0f, 4 );
        TestState to{};
        to.s8 = -100;

        std::array<std::byte, 8> buf{};
        MessageBuf msg{ buf };
        write_field_payload( msg, fld, &to, 0.0 );
        CHECK_EQ( msg.num_bits_written(), 4u );
        CHECK_EQ( byte_at( msg, 0 ), 0x08u );

        TestState decoded{};
        msg.reset();
        read_field_payload( msg, fld, &decoded, 0.0 );
        CHECK_EQ( decoded.s8, -8 );
    }
}

// -- DT_FLOAT with multiplier 8.0, bits=10 ------------------------------------
// write: (int)(12.5 * 8.0) = 100 -> ubit(100,10) -> bytes 64 00
// read: 100 / 8.0 = 12.5f exact

static void test_float_multiplier_golden()
{
    const DeltaField fld = FIELD_OF( f, k_dt_float, 8.0f, 1.0f, 10 );
    TestState to{};
    to.f = 12.5f;

    std::array<std::byte, 8> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, 0.0 );

    CHECK_EQ( msg.num_bits_written(), 10u );
    CHECK_EQ( byte_at( msg, 0 ), 0x64u );
    CHECK_EQ( byte_at( msg, 1 ), 0x00u );

    TestState decoded{};
    msg.reset();
    read_field_payload( msg, fld, &decoded, 0.0 );
    CHECK( decoded.f == 12.5f );
}

// -- DT_FLOAT post_multiplier read-side scaling --------------------------------
// mult=4, post=2: write (int)(3.0*4) = 12; read 12/4 = 3.0f, * 2 = 6.0f

static void test_float_post_multiplier()
{
    const DeltaField fld = FIELD_OF( f, k_dt_float, 4.0f, 2.0f, 16 );
    TestState to{};
    to.f = 3.0f;

    std::array<std::byte, 8> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, 0.0 );

    TestState decoded{};
    msg.reset();
    read_field_payload( msg, fld, &decoded, 0.0 );
    CHECK( decoded.f == 6.0f );
}

// -- DT_ANGLE 16 bits ----------------------------------------------------------
// 90 deg * 65536 / 360 = 16384 = 0x4000 -> LSB-first bytes 00 40
// read: 16384 * 360 / 65536 = 90.0 exact

static void test_angle_16bit_golden()
{
    const DeltaField fld = FIELD_OF( ang, k_dt_angle, 1.0f, 1.0f, 16 );
    TestState to{};
    to.ang = 90.0f;

    std::array<std::byte, 8> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, 0.0 );

    CHECK_EQ( msg.num_bits_written(), 16u );
    CHECK_EQ( byte_at( msg, 0 ), 0x00u );
    CHECK_EQ( byte_at( msg, 1 ), 0x40u );

    TestState decoded{};
    msg.reset();
    read_field_payload( msg, fld, &decoded, 0.0 );
    CHECK( decoded.ang == 90.0f );
}

// -- DT_TIMEWINDOW_8, bits=8 ----------------------------------------------------
// timebase = 123.123, value = 123.100f (as double: 123.09999847...)
// dt = q_rint((123.123 - 123.09999847...) * 100.0) = q_rint(2.30015...) = 2
// -> sbit(2,8) -> byte0 = 0x02
// read: (123.123*100 - 2)/100 = 123.103 (timewindow is quantised, not exact)

static void test_timewindow8_golden()
{
    const DeltaField fld = FIELD_OF( tw8, k_dt_timewindow_8, 100.0f, 1.0f, 8 );
    const double timebase = 123.123;
    TestState to{};
    to.tw8 = 123.100f;

    std::array<std::byte, 8> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, timebase );

    CHECK_EQ( msg.num_bits_written(), 8u );
    CHECK_EQ( byte_at( msg, 0 ), 0x02u );

    TestState decoded{};
    msg.reset();
    read_field_payload( msg, fld, &decoded, timebase );
    CHECK( decoded.tw8 > 123.102f && decoded.tw8 < 123.104f );
}

// -- DT_TIMEWINDOW_BIG with multiplier 1000, bits=16 -----------------------------
// timebase = 5.0, value = 4.9977f (4.99769973...)
// dt = q_rint((5.0 - 4.99769973...) * 1000.0) = q_rint(2.30026...) = 2
// -> sbit(2,16) -> bytes 02 00

static void test_timewindow_big_golden()
{
    const DeltaField fld = FIELD_OF( twbig, k_dt_timewindow_big, 1000.0f, 1.0f, 16 );
    const double timebase = 5.0;
    TestState to{};
    to.twbig = 4.9977f;

    std::array<std::byte, 8> buf{};
    MessageBuf msg{ buf };
    write_field_payload( msg, fld, &to, timebase );

    CHECK_EQ( msg.num_bits_written(), 16u );
    CHECK_EQ( byte_at( msg, 0 ), 0x02u );
    CHECK_EQ( byte_at( msg, 1 ), 0x00u );
}

// -- DT_STRING at an unaligned bit offset -----------------------------------------
// one mark-style bit (1), then "sky\0" shifted left by one bit:
//   byte0 = 1 | (0x73 & 0x7F) << 1 = 0xE7      ('s' = 0x73)
//   byte1 = (0x73 >> 7) | (0x6B << 1) = 0xD6   ('k' = 0x6B)
//   byte2 = (0x6B >> 7) | (0x79 << 1) = 0xF2   ('y' = 0x79)
//   byte3 = (0x79 >> 7) | (0x00 << 1) = 0x00   (NUL)

static void test_string_unaligned_golden()
{
    const DeltaField fld = FIELD_OF( str, k_dt_string, 1.0f, 1.0f, 1 );
    TestState to{};
    std::memcpy( to.str, "sky", 4 );

    std::array<std::byte, 16> buf{};
    MessageBuf msg{ buf };
    msg.write_one_bit( 1 );
    write_field_payload( msg, fld, &to, 0.0 );

    CHECK_EQ( msg.num_bits_written(), 1u + 4u * 8u );
    CHECK_EQ( byte_at( msg, 0 ), 0xE7u );
    CHECK_EQ( byte_at( msg, 1 ), 0xD6u );
    CHECK_EQ( byte_at( msg, 2 ), 0xF2u );
    CHECK_EQ( byte_at( msg, 3 ), 0x00u );

    TestState decoded{};
    msg.reset();
    CHECK_EQ( msg.read_one_bit(), 1 );
    read_field_payload( msg, fld, &decoded, 0.0 );
    CHECK_STREQ( decoded.str, "sky" );
}

// -- float compare semantics: raw bit patterns -------------------------------------

static void test_float_compare_raw_bits()
{
    const DeltaField fld = FIELD_OF( f, k_dt_float, 1.0f, 1.0f, 32 );
    TestState a{}, b{};

    a.f = 0.0f;
    b.f = -0.0f; // same value, different bit pattern -> counts as changed
    CHECK( !compare_field( fld, &a, &b ));

    b.f = 0.0f;
    CHECK( compare_field( fld, &a, &b ));
}

// -- timewindow compare is quantised --------------------------------------------

static void test_timewindow_compare_quantised()
{
    const DeltaField fld = FIELD_OF( tw8, k_dt_timewindow_8, 100.0f, 1.0f, 8 );
    TestState a{}, b{};

    // both quantise to q_rint(x*100) = 123 -> unchanged despite float diff
    a.tw8 = 1.234f;
    b.tw8 = 1.2341f;
    CHECK( compare_field( fld, &a, &b ));

    b.tw8 = 1.30f; // 130 != 123 -> changed
    CHECK( !compare_field( fld, &a, &b ));
}

// -- inactive fields always compare equal ------------------------------------------

static void test_inactive_field_compares_equal()
{
    DeltaField fld = FIELD_OF( u32, k_dt_integer, 1.0f, 1.0f, 32 );
    TestState a{}, b{};
    b.u32 = 777;

    CHECK( !compare_field( fld, &a, &b ));
    fld.inactive = true;
    CHECK( compare_field( fld, &a, &b ));
}

// -- copy_field --------------------------------------------------------------------

static void test_copy_field()
{
    TestState from{}, to{};
    from.s32 = -42;
    std::memcpy( from.str, "map", 4 );

    copy_field( FIELD_OF( s32, k_dt_integer | k_dt_signed, 1.0f, 1.0f, 32 ),
                &from, &to );
    CHECK_EQ( to.s32, -42 );

    copy_field( FIELD_OF( str, k_dt_string, 1.0f, 1.0f, 1 ), &from, &to );
    CHECK_STREQ( to.str, "map" );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_clamp_integer_field );
    RUN_TEST( test_byte_unsigned_golden );
    RUN_TEST( test_short_signed_golden );
    RUN_TEST( test_integer_multiplier_chain );
    RUN_TEST( test_write_clamps );
    RUN_TEST( test_float_multiplier_golden );
    RUN_TEST( test_float_post_multiplier );
    RUN_TEST( test_angle_16bit_golden );
    RUN_TEST( test_timewindow8_golden );
    RUN_TEST( test_timewindow_big_golden );
    RUN_TEST( test_string_unaligned_golden );
    RUN_TEST( test_float_compare_raw_bits );
    RUN_TEST( test_timewindow_compare_quantised );
    RUN_TEST( test_inactive_field_compares_equal );
    RUN_TEST( test_copy_field );

    std::printf( "delta_field_codec: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
