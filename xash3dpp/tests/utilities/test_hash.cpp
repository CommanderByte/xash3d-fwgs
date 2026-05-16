// xash3dpp — hash utility tests
// Covers: CRC32 one-shot, CRC32 incremental, MD5

#include <xash3dpp/utilities/hash.hpp>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

static void test_crc32_empty()
{
    // CRC32 of empty string: standard IEEE 802.3 value is 0x00000000
    xash::utilities::Crc32 s;
    xash::utilities::crc32_init( s );
    CHECK( xash::utilities::crc32_final( s ) == 0x00000000u );
}

static void test_crc32_known()
{
    // CRC32("123456789") == 0xCBF43926 per the standard test vector.
    const char *data = "123456789";
    xash::utilities::Crc32 result = xash::utilities::crc32( data, std::strlen(data) );
    CHECK( result == 0xCBF43926u );
}

static void test_crc32_incremental()
{
    // Incremental feed must match one-shot.
    const char *data = "Hello, World!";
    std::size_t len  = std::strlen( data );

    xash::utilities::Crc32 s;
    xash::utilities::crc32_init( s );
    for( std::size_t i = 0; i < len; ++i )
        xash::utilities::crc32_update( s, static_cast<std::uint8_t>( data[i] ) );
    xash::utilities::Crc32 incremental = xash::utilities::crc32_final( s );

    xash::utilities::Crc32 oneshot = xash::utilities::crc32( data, len );
    CHECK( incremental == oneshot );
}

int main()
{
    RUN_TEST( test_crc32_empty );
    RUN_TEST( test_crc32_known );
    RUN_TEST( test_crc32_incremental );

    std::printf( "hash: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
