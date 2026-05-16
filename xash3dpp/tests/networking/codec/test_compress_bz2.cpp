// xash3dpp — bz2 link-time-selection smoke test
// Verifies that exactly one of compress_bz2.cpp / compress_null.cpp is linked
// in (no duplicate-symbol errors at link time), that the public bz2 surface
// is callable, and that both backends currently surface NotInitialised
// (real bzip2 wiring lands later — see compress_bz2.cpp).
//
// Once compress_bz2.cpp gains a real bzip2 backend, the
// `available() == true` branch below should switch to a round-trip
// expectation.

#include <xash3dpp/private/networking/codec/compress.hpp>

#include "../../test_helpers.hpp"

#include <array>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking::bz2;
using xash::networking::NetError;

static void test_available_matches_link_choice()
{
    // Both stub backends currently report false.  This test is the canary
    // for the day compress_bz2.cpp gains a real implementation: flip it to
    // CHECK( available() ) under XASH_NET_COMPRESSION and add the
    // round-trip below at the same time.
    CHECK( !available() );
}

static void test_compress_returns_not_initialised()
{
    std::array<std::byte, 32> src{};
    auto r = compress( src );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::NotInitialised );
}

static void test_decompress_returns_not_initialised()
{
    std::array<std::byte, 32> src{};
    std::array<std::byte, 64> dst{};
    auto r = decompress( src, dst );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::NotInitialised );
}

int main()
{
    test_available_matches_link_choice();
    test_compress_returns_not_initialised();
    test_decompress_returns_not_initialised();

    std::printf( "test_compress_bz2: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
