// xash3dpp — OOB packet tests

#include <xash3dpp/private/networking/oob_packet.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstring>
#include <string_view>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

static void test_encode_decode_string()
{
    std::array<std::byte, 64> buf{};
    auto n = oob::encode( std::string_view{ "getchallenge" }, buf );
    CHECK( n.has_value() );
    CHECK_EQ( static_cast<int>( *n ), 4 + 12 );

    // Magic bytes are FF FF FF FF.
    CHECK( buf[0] == std::byte{ 0xFF } );
    CHECK( buf[1] == std::byte{ 0xFF } );
    CHECK( buf[2] == std::byte{ 0xFF } );
    CHECK( buf[3] == std::byte{ 0xFF } );

    auto packet = std::span<const std::byte>{ buf }.subspan( 0, *n );
    CHECK( oob::is_oob( packet ) );

    auto payload = oob::decode( packet );
    CHECK( payload.has_value() );
    CHECK_EQ( static_cast<int>( payload->size() ), 12 );
    CHECK( std::memcmp( payload->data(), "getchallenge", 12 ) == 0 );
}

static void test_encode_empty_payload()
{
    std::array<std::byte, 8> buf{};
    auto n = oob::encode( std::span<const std::byte>{}, buf );
    CHECK( n.has_value() );
    CHECK_EQ( static_cast<int>( *n ), 4 );

    auto payload = oob::decode( std::span<const std::byte>{ buf }.subspan( 0, 4 ) );
    CHECK( payload.has_value() );
    CHECK( payload->empty() );
}

static void test_encode_dst_too_small()
{
    std::array<std::byte, 4> tiny{};
    auto r = oob::encode( std::string_view{ "x" }, tiny );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BufferTooSmall );
}

static void test_decode_rejects_short_packet()
{
    std::array<std::byte, 3> shorty{ std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF } };
    CHECK( !oob::is_oob( shorty ) );
    auto r = oob::decode( shorty );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BadAddress );
}

static void test_decode_rejects_wrong_magic()
{
    std::array<std::byte, 8> buf{ std::byte{ 0xFE }, std::byte{ 0xFF },
                                  std::byte{ 0xFF }, std::byte{ 0xFF },
                                  std::byte{ 1 }, std::byte{ 2 },
                                  std::byte{ 3 }, std::byte{ 4 } };
    CHECK( !oob::is_oob( buf ) );
    auto r = oob::decode( buf );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BadAddress );
}

static void test_encode_binary_payload()
{
    const std::byte payload[] = { std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0xAA } };
    std::array<std::byte, 32> buf{};
    auto n = oob::encode( payload, buf );
    CHECK( n.has_value() );
    auto p = oob::decode( std::span<const std::byte>{ buf }.subspan( 0, *n ) );
    CHECK( p.has_value() );
    CHECK( std::memcmp( p->data(), payload, sizeof( payload ) ) == 0 );
}

int main()
{
    std::printf( "test_oob_packet\n" );
    RUN_TEST( test_encode_decode_string );
    RUN_TEST( test_encode_empty_payload );
    RUN_TEST( test_encode_dst_too_small );
    RUN_TEST( test_decode_rejects_short_packet );
    RUN_TEST( test_decode_rejects_wrong_magic );
    RUN_TEST( test_encode_binary_payload );
    std::printf( "test_oob_packet: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
