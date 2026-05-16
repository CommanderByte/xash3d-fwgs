// xash3dpp — NetAddress parser/format/compare tests

#include <xash3dpp/networking/address.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstring>

static int g_pass = 0, g_fail = 0;

using xash::networking::NetAddress;
using xash::networking::IpFamily;
using xash::networking::NetError;
using xash::networking::from_string;
using xash::networking::to_string;
using xash::networking::compare_base;
using xash::networking::mask_compare;

static void test_parse_basic()
{
    auto a = from_string( "192.168.1.42:27015" );
    CHECK( a.has_value() );
    CHECK_EQ( static_cast<int>( a->addr.v4[0] ), 192 );
    CHECK_EQ( static_cast<int>( a->addr.v4[1] ), 168 );
    CHECK_EQ( static_cast<int>( a->addr.v4[2] ), 1 );
    CHECK_EQ( static_cast<int>( a->addr.v4[3] ), 42 );
    CHECK_EQ( static_cast<int>( a->port ), 27015 );
}

static void test_parse_no_port()
{
    auto a = from_string( "10.0.0.1" );
    CHECK( a.has_value() );
    CHECK_EQ( static_cast<int>( a->port ), 0 );
}

static void test_parse_rejects_octet_overflow()
{
    auto a = from_string( "256.0.0.1" );
    CHECK( !a.has_value() );
    CHECK( a.error() == NetError::BadAddress );
}

static void test_parse_rejects_missing_octet()
{
    auto a = from_string( "1.2.3" );
    CHECK( !a.has_value() );
}

static void test_parse_rejects_v6_literal()
{
    auto a = from_string( "[::1]:27015" );
    CHECK( !a.has_value() );
    CHECK( a.error() == NetError::BadAddress );
}

static void test_parse_rejects_trailing_junk()
{
    auto a = from_string( "1.2.3.4:5x" );
    CHECK( !a.has_value() );
}

static void test_format()
{
    auto src = from_string( "127.0.0.1:80" );
    CHECK( src.has_value() );
    std::array<char, 32> buf{};
    auto n = to_string( *src, buf );
    CHECK( n.has_value() );
    CHECK( std::strcmp( buf.data(), "127.0.0.1:80" ) == 0 );
}

static void test_format_buffer_too_small()
{
    NetAddress a = NetAddress::loopback_v4( 1 );
    std::array<char, 8> tiny{};
    auto r = to_string( a, tiny );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BufferTooSmall );
}

static void test_compare_base()
{
    auto a = from_string( "10.0.0.5:1000" );
    auto b = from_string( "10.0.0.5:2000" );
    CHECK( a.has_value() && b.has_value() );
    CHECK( compare_base( *a, *b ) );
    CHECK( *a != *b );
}

static void test_mask_compare()
{
    auto a = from_string( "10.20.30.40" );
    auto b = from_string( "10.20.30.99" );
    auto c = from_string( "10.21.30.40" );
    CHECK( a.has_value() && b.has_value() && c.has_value() );
    CHECK( mask_compare( *a, *b, 24 ) );
    CHECK( !mask_compare( *a, *c, 16 ) );           // 10.20 vs 10.21 differ
    CHECK( !mask_compare( *a, *c, 24 ) );
    CHECK( mask_compare( *a, *c, 8 ) );             // /8 matches (both 10.x)
    CHECK( mask_compare( *a, *c, 0 ) );             // /0 always matches
}

int main()
{
    std::printf( "test_netadr\n" );
    RUN_TEST( test_parse_basic );
    RUN_TEST( test_parse_no_port );
    RUN_TEST( test_parse_rejects_octet_overflow );
    RUN_TEST( test_parse_rejects_missing_octet );
    RUN_TEST( test_parse_rejects_v6_literal );
    RUN_TEST( test_parse_rejects_trailing_junk );
    RUN_TEST( test_format );
    RUN_TEST( test_format_buffer_too_small );
    RUN_TEST( test_compare_base );
    RUN_TEST( test_mask_compare );
    std::printf( "test_netadr: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
