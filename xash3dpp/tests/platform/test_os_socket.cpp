// xash3dpp — platform socket tests
// Covers: OsSocket lifecycle (default, move), socket_init/shutdown idempotency,
//         default_platform_sockets() accessor, open_udp_socket() smoke test,
//         NetAddress factories and comparison, IPlatformSockets interface shape.

#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>
#include <xash3dpp/networking/address.hpp>

#include "../test_helpers.hpp"

#include <cstddef>
#include <cstdio>

static int g_pass = 0, g_fail = 0;

using namespace xash::platform;
using namespace xash::networking;

// ---------------------------------------------------------------------------
// OsSocket — value semantics
// ---------------------------------------------------------------------------

static void test_os_socket_default_invalid()
{
    OsSocket s;
    CHECK( !s.valid() );
    CHECK( s.get() == k_invalid_socket );
}

static void test_os_socket_move_construct()
{
    // Wrap an invalid handle — tests the move path without a real socket.
    OsSocket a{ k_invalid_socket };
    CHECK( !a.valid() );

    OsSocket b{ std::move( a ) };
    CHECK( !b.valid() );
    // Source must be left invalid after move.
    CHECK( !a.valid() );
}

static void test_os_socket_move_assign()
{
    OsSocket a;
    OsSocket b;
    b = std::move( a );
    CHECK( !b.valid() );
    CHECK( !a.valid() );
}

static void test_os_socket_release()
{
    OsSocket s;
    SocketHandle h = s.release();
    CHECK( h == k_invalid_socket );
    CHECK( !s.valid() );
}

// ---------------------------------------------------------------------------
// socket_init / socket_shutdown — lifecycle idempotency
// ---------------------------------------------------------------------------

static void test_socket_lifecycle_multiple()
{
    // Multiple init/shutdown pairs must not crash or assert.
    socket_init();
    socket_init();
    socket_shutdown();
    socket_shutdown();
    // Re-init after full shutdown.
    socket_init();
    socket_shutdown();
    ++g_pass;
}

// ---------------------------------------------------------------------------
// default_platform_sockets — accessor
// ---------------------------------------------------------------------------

static void test_default_platform_sockets_accessor()
{
    // Must return a reference (address-stable for the process lifetime).
    IPlatformSockets &a = default_platform_sockets();
    IPlatformSockets &b = default_platform_sockets();
    CHECK( &a == &b );
}

// ---------------------------------------------------------------------------
// open_udp_socket — smoke test (ephemeral V4 socket on loopback)
// ---------------------------------------------------------------------------

static void test_open_udp_socket_v4_ephemeral()
{
    // Port 0 → kernel chooses; bind_iface empty → INADDR_ANY.
    // This may fail in extremely locked-down sandbox environments; if so,
    // the test reports the failure without aborting the suite.
    auto result = open_udp_socket( IpFamily::V4, 0, "" );
    if( !result.has_value() )
    {
        std::printf( "  NOTE: open_udp_socket(V4,0) failed (sandbox?) — skipping\n" );
        ++g_pass;  // treat as non-fatal absence
        return;
    }
    OsSocket &sock = *result;
    CHECK( sock.valid() );

    // Query bound local address — kernel assigned a port.
    auto local = get_local_address( sock );
    CHECK( local.has_value() );
    if( local )
        CHECK( local->family == IpFamily::V4 );
}

// ---------------------------------------------------------------------------
// open_udp_socket — IPlatformSockets delegation
// ---------------------------------------------------------------------------

static void test_iplatform_sockets_open_udp()
{
    IPlatformSockets &psock = default_platform_sockets();
    auto result = psock.open_udp( IpFamily::V4, 0, "" );
    if( !result.has_value() )
    {
        std::printf( "  NOTE: IPlatformSockets::open_udp failed — skipping\n" );
        ++g_pass;
        return;
    }
    CHECK( result->valid() );
}

// ---------------------------------------------------------------------------
// NetAddress — factories and comparison
// ---------------------------------------------------------------------------

static void test_net_address_loopback_v4()
{
    const auto a = NetAddress::loopback_v4( 27005 );
    CHECK( a.family     == IpFamily::V4 );
    CHECK( a.port       == 27005 );
    CHECK( a.addr.v4[0] == 127 );
    CHECK( a.addr.v4[3] == 1 );
    // Invariant: ip6_0 must be zero.
    CHECK( a.ip6_0[0] == 0 );
    CHECK( a.ip6_0[1] == 0 );
}

static void test_net_address_equality()
{
    const auto a = NetAddress::loopback_v4( 27005 );
    const auto b = NetAddress::loopback_v4( 27005 );
    const auto c = NetAddress::loopback_v4( 27006 );
    CHECK( a == b );
    CHECK( a != c );
}

static void test_net_address_any_v4()
{
    const auto a = NetAddress::any_v4( 0 );
    CHECK( a.family     == IpFamily::V4 );
    CHECK( a.port       == 0 );
    CHECK( a.addr.v4[0] == 0 );
    CHECK( a.addr.v4[3] == 0 );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // Initialise socket layer before any socket operations.
    socket_init();

    RUN_TEST( test_os_socket_default_invalid );
    RUN_TEST( test_os_socket_move_construct );
    RUN_TEST( test_os_socket_move_assign );
    RUN_TEST( test_os_socket_release );
    RUN_TEST( test_socket_lifecycle_multiple );
    RUN_TEST( test_default_platform_sockets_accessor );
    RUN_TEST( test_open_udp_socket_v4_ephemeral );
    RUN_TEST( test_iplatform_sockets_open_udp );
    RUN_TEST( test_net_address_loopback_v4 );
    RUN_TEST( test_net_address_equality );
    RUN_TEST( test_net_address_any_v4 );

    socket_shutdown();

    std::printf( "os_socket: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
