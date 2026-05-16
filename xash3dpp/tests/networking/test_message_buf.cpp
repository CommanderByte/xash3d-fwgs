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
    std::printf( "test_message_buf: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
