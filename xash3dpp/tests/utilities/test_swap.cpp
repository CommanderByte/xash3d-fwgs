// xash3dpp — byte-swap tests
// Covers: swap_bytes (2, 4, 8 bytes)
// Note: swap_struct is declared but has no implementation yet — not tested here.

#include <xash3dpp/utilities/swap.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

static void test_swap_bytes_2()
{
    // legacy: BigShort / LittleShort via swaplib.h
    std::uint16_t v = 0x1234u;
    xash::utilities::swap_bytes( &v, 2 );
    CHECK( v == 0x3412u );

    // Double-swap restores original.
    xash::utilities::swap_bytes( &v, 2 );
    CHECK( v == 0x1234u );
}

static void test_swap_bytes_4()
{
    // legacy: BigLong / LittleLong
    std::uint32_t v = 0x12345678u;
    xash::utilities::swap_bytes( &v, 4 );
    CHECK( v == 0x78563412u );

    xash::utilities::swap_bytes( &v, 4 );
    CHECK( v == 0x12345678u );
}

static void test_swap_bytes_8()
{
    std::uint64_t v = 0x0102030405060708ull;
    xash::utilities::swap_bytes( &v, 8 );
    CHECK( v == 0x0807060504030201ull );

    xash::utilities::swap_bytes( &v, 8 );
    CHECK( v == 0x0102030405060708ull );
}

static void test_swap_bytes_noop()
{
    // Size 1 — default branch, value unchanged.
    std::uint8_t v = 0xABu;
    xash::utilities::swap_bytes( &v, 1 );
    CHECK( v == 0xABu );

    // Unsupported size 3 — value unchanged.
    unsigned char buf[3] = { 0x01, 0x02, 0x03 };
    xash::utilities::swap_bytes( buf, 3 );
    CHECK( buf[0] == 0x01 && buf[1] == 0x02 && buf[2] == 0x03 );
}

int main()
{
    RUN_TEST( test_swap_bytes_2 );
    RUN_TEST( test_swap_bytes_4 );
    RUN_TEST( test_swap_bytes_8 );
    RUN_TEST( test_swap_bytes_noop );

    std::printf( "swap: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
