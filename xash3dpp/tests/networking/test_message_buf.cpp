// xash3dpp — MessageBuf round-trip test
// Covers byte primitives, bit packing, signed bit packing, strings,
// overflow detection, and seek.

#include <xash3dpp/networking/message_buf.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstring>

static int g_pass = 0, g_fail = 0;

using xash::networking::MessageBuf;
using xash::networking::SeekOrigin;

static void test_byte_roundtrip()
{
    std::array<std::byte, 64> storage{};
    MessageBuf w( storage, "w" );
    w.write_byte( 0xAB );
    w.write_char( -42 );
    w.write_word( 0xBEEF );
    w.write_short( -12345 );
    w.write_dword( 0xDEADBEEFu );
    w.write_long( -987654321 );
    w.write_float( 3.14159f );
    CHECK( !w.overflowed() );

    MessageBuf r( storage, "r" );
    CHECK_EQ( static_cast<int>( r.read_byte() ),  0xAB );
    CHECK_EQ( static_cast<int>( r.read_char() ),  -42 );
    CHECK_EQ( static_cast<int>( r.read_word() ),  0xBEEF );
    CHECK_EQ( static_cast<int>( r.read_short() ), -12345 );
    CHECK_EQ( static_cast<uint32_t>( r.read_dword() ), 0xDEADBEEFu );
    CHECK_EQ( static_cast<int32_t>( r.read_long() ),   -987654321 );
    CHECK( r.read_float() == 3.14159f );
    CHECK( !r.overflowed() );
}

static void test_bit_packing()
{
    std::array<std::byte, 16> storage{};
    MessageBuf w( storage );
    w.write_one_bit( 1 );
    w.write_one_bit( 0 );
    w.write_one_bit( 1 );
    w.write_ubit_long( 0x1F, 5 ); // 5 bits
    w.write_ubit_long( 0xABCD, 16 );
    CHECK( !w.overflowed() );

    MessageBuf r( storage );
    CHECK_EQ( r.read_one_bit(), 1 );
    CHECK_EQ( r.read_one_bit(), 0 );
    CHECK_EQ( r.read_one_bit(), 1 );
    CHECK_EQ( static_cast<int>( r.read_ubit_long( 5 ) ),  0x1F );
    CHECK_EQ( static_cast<int>( r.read_ubit_long( 16 ) ), 0xABCD );
    CHECK( !r.overflowed() );
}

static void test_signed_bit_packing()
{
    std::array<std::byte, 8> storage{};
    MessageBuf w( storage );
    w.write_sbit_long( -1,  8 );
    w.write_sbit_long( -16, 6 );
    w.write_sbit_long( 31,  6 );

    MessageBuf r( storage );
    CHECK_EQ( r.read_sbit_long( 8 ), -1 );
    CHECK_EQ( r.read_sbit_long( 6 ), -16 );
    CHECK_EQ( r.read_sbit_long( 6 ),  31 );
    CHECK( !r.overflowed() );
}

static void test_string()
{
    std::array<std::byte, 32> storage{};
    MessageBuf w( storage );
    CHECK( w.write_string( "hello" ) );

    MessageBuf r( storage );
    char out[32];
    const std::size_t n = r.read_string( out );
    CHECK_EQ( static_cast<int>( n ), 5 );
    CHECK( std::strcmp( out, "hello" ) == 0 );
}

static void test_overflow_write()
{
    std::array<std::byte, 2> storage{};
    MessageBuf w( storage );
    w.write_dword( 0xCAFEBABEu );
    CHECK( w.overflowed() );
}

static void test_overflow_read()
{
    std::array<std::byte, 2> storage{};
    MessageBuf r( storage );
    (void) r.read_dword();
    CHECK( r.overflowed() );
}

static void test_seek()
{
    std::array<std::byte, 32> storage{};
    MessageBuf w( storage );
    w.write_byte( 0x11 );
    w.write_byte( 0x22 );
    w.write_byte( 0x33 );

    MessageBuf r( storage );
    CHECK_EQ( static_cast<int>( r.read_byte() ), 0x11 );
    CHECK( r.seek_to_bit( 16, SeekOrigin::Begin ) );
    CHECK_EQ( static_cast<int>( r.read_byte() ), 0x33 );
    CHECK( r.seek_to_bit( -8, SeekOrigin::Current ) );
    CHECK_EQ( static_cast<int>( r.read_byte() ), 0x33 );
    CHECK( !r.seek_to_bit( 9999, SeekOrigin::Begin ) );
}

static void test_bytes_roundtrip()
{
    std::array<std::byte, 16> storage{};
    const std::byte payload[] = {
        std::byte{ 0xDE }, std::byte{ 0xAD }, std::byte{ 0xBE }, std::byte{ 0xEF }
    };
    MessageBuf w( storage );
    CHECK( w.write_bytes( payload ) );

    MessageBuf r( storage );
    std::byte out[4]{};
    CHECK( r.read_bytes( out ) );
    CHECK_EQ( std::to_integer<int>( out[0] ), 0xDE );
    CHECK_EQ( std::to_integer<int>( out[3] ), 0xEF );
}

static bool nearly_equal( float a, float b, float tol )
{
    const float d = a - b;
    return ( d < 0 ? -d : d ) <= tol;
}

static void test_coord_roundtrip()
{
    std::array<std::byte, 16> storage{};
    MessageBuf w( storage );
    w.write_coord( 0.0f );
    w.write_coord( 1.0f );
    w.write_coord( -123.5f );
    w.write_coord( 4095.875f );        // max representable: 32767/8
    CHECK( !w.overflowed() );

    MessageBuf r( storage );
    CHECK( nearly_equal( r.read_coord(),     0.0f,     1.0f / 8.0f ) );
    CHECK( nearly_equal( r.read_coord(),     1.0f,     1.0f / 8.0f ) );
    CHECK( nearly_equal( r.read_coord(),  -123.5f,     1.0f / 8.0f ) );
    CHECK( nearly_equal( r.read_coord(),  4095.875f,   1.0f / 8.0f ) );
}

static void test_coord_large_roundtrip()
{
    std::array<std::byte, 8> storage{};
    MessageBuf w( storage );
    w.write_coord_large(  12345.0f );
    w.write_coord_large( -32000.4f );  // truncates to -32000

    MessageBuf r( storage );
    CHECK( nearly_equal( r.read_coord_large(),  12345.0f, 1.0f ) );
    CHECK( nearly_equal( r.read_coord_large(), -32000.0f, 1.0f ) );
}

static void test_bit_angle_roundtrip()
{
    std::array<std::byte, 16> storage{};
    MessageBuf w( storage );
    w.write_bit_angle(    0.0f, 8 );
    w.write_bit_angle(   90.0f, 16 );
    w.write_bit_angle( -179.0f, 16 );  // wraps to 181
    w.write_bit_angle(  360.5f, 16 );  // wraps to 0.5
    CHECK( !w.overflowed() );

    MessageBuf r( storage );
    // 8-bit quantisation -> ~1.4 deg LSB
    CHECK( nearly_equal( r.read_bit_angle(  8 ),   0.0f, 2.0f ) );
    CHECK( nearly_equal( r.read_bit_angle( 16 ),  90.0f, 0.01f ) );
    CHECK( nearly_equal( r.read_bit_angle( 16 ), -179.0f, 0.01f ) );
    CHECK( nearly_equal( r.read_bit_angle( 16 ),   0.5f, 0.01f ) );
}

static void test_vec3()
{
    std::array<std::byte, 32> storage{};
    MessageBuf w( storage );
    w.write_vec3_coord( 1.0f, -2.5f, 3.125f );
    w.write_vec3_angles( 10.0f, 90.0f, -170.0f );

    MessageBuf r( storage );
    float x = 0, y = 0, z = 0;
    r.read_vec3_coord( x, y, z );
    CHECK( nearly_equal( x,  1.0f,   1.0f / 8.0f ) );
    CHECK( nearly_equal( y, -2.5f,   1.0f / 8.0f ) );
    CHECK( nearly_equal( z,  3.125f, 1.0f / 8.0f ) );
    r.read_vec3_angles( x, y, z );
    CHECK( nearly_equal( x,   10.0f, 0.01f ) );
    CHECK( nearly_equal( y,   90.0f, 0.01f ) );
    CHECK( nearly_equal( z, -170.0f, 0.01f ) );
}

int main()
{
    std::printf( "test_message_buf\n" );
    RUN_TEST( test_byte_roundtrip );
    RUN_TEST( test_bit_packing );
    RUN_TEST( test_signed_bit_packing );
    RUN_TEST( test_string );
    RUN_TEST( test_overflow_write );
    RUN_TEST( test_overflow_read );
    RUN_TEST( test_seek );
    RUN_TEST( test_bytes_roundtrip );
    RUN_TEST( test_coord_roundtrip );
    RUN_TEST( test_coord_large_roundtrip );
    RUN_TEST( test_bit_angle_roundtrip );
    RUN_TEST( test_vec3 );
    std::printf( "test_message_buf: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
