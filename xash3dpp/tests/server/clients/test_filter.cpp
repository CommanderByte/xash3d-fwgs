// xash3dpp — ban-filter tests (Chunk 6 S9 satellite).
// Pins SV_CheckID mutual-prefix matching + lazy expiry, and SV_CheckIP CIDR
// containment (sv_filter.c parity + the engine self-tests' inclusion logic).

#include <xash3dpp/private/server/clients.hpp>

#include <xash3dpp/networking/address.hpp>

#include "../../test_helpers.hpp"

namespace sv  = xash::server;
namespace net = xash::networking;

static int g_pass = 0, g_fail = 0;

static net::NetAddress v4( unsigned a, unsigned b, unsigned c, unsigned d,
                           std::uint16_t port = 0 )
{
    net::NetAddress adr{};
    adr.family     = net::IpFamily::V4;
    adr.port       = port;
    adr.addr.v4[0] = static_cast<std::uint8_t>( a );
    adr.addr.v4[1] = static_cast<std::uint8_t>( b );
    adr.addr.v4[2] = static_cast<std::uint8_t>( c );
    adr.addr.v4[3] = static_cast<std::uint8_t>( d );
    return adr;
}

static void test_id_prefix_match()
{
    sv::ClientMachinery cm;
    cm.realtime = 100.0;

    sv::filter_add_id( cm, 0.0f, "STEAM_0:1:12345" ); // permanent

    // exact + prefix (mutual-prefix rule) match.
    CHECK( sv::filter_check_id( cm, "STEAM_0:1:12345" ) );
    CHECK( sv::filter_check_id( cm, "STEAM_0:1:12345_extra" ) ); // query is longer
    CHECK( !sv::filter_check_id( cm, "STEAM_9:9:9" ) );
    CHECK( !sv::filter_check_id( cm, "" ) );

    sv::filter_remove_id( cm, "STEAM_0:1:12345" );
    CHECK( !sv::filter_check_id( cm, "STEAM_0:1:12345" ) );
}

static void test_id_expiry()
{
    sv::ClientMachinery cm;
    cm.realtime = 100.0;
    sv::filter_add_id( cm, 1.0f, "TEMPBAN" ); // 1 minute → expiry 160

    CHECK( sv::filter_check_id( cm, "TEMPBAN" ) );
    cm.realtime = 200.0; // past expiry
    CHECK( !sv::filter_check_id( cm, "TEMPBAN" ) ); // lazily pruned
}

static void test_ip_cidr()
{
    sv::ClientMachinery cm;
    cm.realtime = 100.0;

    // ban the whole 192.168.1.0/24
    sv::filter_add_ip( cm, 0.0f, v4( 192, 168, 1, 0 ), 24 );

    CHECK( sv::filter_check_ip( cm, v4( 192, 168, 1, 55, 27015 ) ) );
    CHECK( sv::filter_check_ip( cm, v4( 192, 168, 1, 200 ) ) );
    CHECK( !sv::filter_check_ip( cm, v4( 192, 168, 2, 1 ) ) );
    CHECK( !sv::filter_check_ip( cm, v4( 10, 0, 0, 1 ) ) );

    // removeip of the covering mask clears the /24 entry.
    sv::filter_remove_ip( cm, v4( 192, 168, 1, 0 ), 24 );
    CHECK( !sv::filter_check_ip( cm, v4( 192, 168, 1, 55 ) ) );
}

static void test_ip_host_ban()
{
    sv::ClientMachinery cm;
    cm.realtime = 100.0;
    sv::filter_add_ip( cm, 0.0f, v4( 5, 6, 7, 8 ), 0 ); // cidr 0 ⇒ /32 host

    CHECK( sv::filter_check_ip( cm, v4( 5, 6, 7, 8, 1234 ) ) );
    CHECK( !sv::filter_check_ip( cm, v4( 5, 6, 7, 9 ) ) );
}

int main()
{
    RUN_TEST( test_id_prefix_match );
    RUN_TEST( test_id_expiry );
    RUN_TEST( test_ip_cidr );
    RUN_TEST( test_ip_host_ban );
    std::printf( "server_filter: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
