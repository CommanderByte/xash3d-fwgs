// xash3dpp — client connection state-machine tests (Chunk 6 S9).
// Drives challenge → connect → new → spawn → begin → drop → timeout end-to-end
// against the real fake game DLL + a spawned synthetic map, pinning the
// game-callback fan-out (ClientConnect / PutInServer / Command / UserInfoChanged
// / Disconnect), the stateless MD5 challenge, and SV_FakeConnect.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>
#include <xash3dpp/private/server/clients.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/utilities/string.hpp>

#include "../abi/fake_dll_state.hpp"
#include "../../map_loader/bsp/test_bsp_builder.hpp"
#include "../../cmd_cvar/test_stubs.hpp"

#include "../../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace cc  = xash::cmd_cvar;
namespace net = xash::networking;

using xash::platform::IpFamily;
using xash::platform::IPlatformSockets;
using xash::platform::OsSocket;

static int g_pass = 0, g_fail = 0;

static std::filesystem::path g_root;

static const char *k_delta_lst =
    // usercmd_t is required by clc_move parsing (read_delta_usercmd); a real
    // HL delta.lst defines it — this mirrors the minimal codec-test block.
    "usercmd_t none\n"
    "{\n"
    "    DEFINE_DELTA( msec, DT_BYTE, 8, 1.0 ),\n"
    "    DEFINE_DELTA( buttons, DT_SHORT, 16, 1.0 ),\n"
    "    DEFINE_DELTA( forwardmove, DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
    "    DEFINE_DELTA( viewangles[1], DT_ANGLE, 16, 1.0 )\n"
    "}\n"
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n"
    "}\n";

static const char *k_spawn_entities =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "\"message\" \"conn test\"\n"
    "}\n"
    "{\n"
    "\"classname\" \"info_player_start\"\n"
    "\"origin\" \"0 0 0\"\n"
    "}\n";

static void write_text( const std::filesystem::path &p, const std::string &s )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( s.data(), static_cast<std::streamsize>( s.size() ) );
}

static void write_bytes( const std::filesystem::path &p,
                         const std::vector<std::byte> &b )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( reinterpret_cast<const char *>( b.data() ),
             static_cast<std::streamsize>( b.size() ) );
}

static void setup_tree()
{
    g_root = std::filesystem::temp_directory_path() / "xash3dpp_clients_test";
    std::filesystem::remove_all( g_root );
    std::filesystem::create_directories( g_root / "valve" );
    write_text( g_root / "game" / "delta.lst", k_delta_lst );

    auto builder = test_bsp::make_minimal_world();
    builder.set_entities( k_spawn_entities );
    write_bytes( g_root / "game" / "maps" / "conntest.bsp", builder.build() );
}

// Captures the connectionless OOB replies the state machine emits.
struct CaptureSink final : sv::IOobSink
{
    char last[1024] = {};
    int  count      = 0;
    void send_oob( const net::NetAddress &, const char *text ) noexcept override
    {
        ++count;
        xash::utilities::strncpy( last, text ? text : "", sizeof( last ) );
    }
};

// A do-nothing platform sockets layer: NetworkContext::init only creates its
// fragment pool (no socket syscalls) and these tests never transmit, so
// open_udp just hands back a valid-looking handle and the rest are inert.
struct FakeSockets final : IPlatformSockets
{
    // captured outbound datagram (for the send-path test).
    int             sendto_calls = 0;
    net::NetAddress last_dest{};
    std::size_t     last_len = 0;

    net::Result<OsSocket> open_udp( IpFamily, std::uint16_t,
                                    std::string_view ) noexcept override
    {
        return OsSocket{ static_cast<xash::platform::SocketHandle>( 0x42 ) };
    }
    net::Result<OsSocket> open_tcp( IpFamily ) noexcept override
    {
        return std::unexpected( net::NetError::NotInitialised );
    }
    net::Result<std::size_t> sendto( const OsSocket &, std::span<const std::byte> data,
                                     const net::NetAddress &to ) noexcept override
    {
        ++sendto_calls;
        last_dest = to;
        last_len  = data.size();
        return data.size();
    }
    net::Result<std::size_t> recvfrom( const OsSocket &, std::span<std::byte>,
                                       net::NetAddress & ) noexcept override
    {
        return std::unexpected( net::NetError::WouldBlock );
    }
    net::Result<std::size_t> send_stream( const OsSocket &,
                                          std::span<const std::byte> ) noexcept override
    {
        return std::unexpected( net::NetError::NotInitialised );
    }
    net::Result<std::size_t> recv_stream( const OsSocket &,
                                          std::span<std::byte> ) noexcept override
    {
        return std::unexpected( net::NetError::NotInitialised );
    }
    net::Result<void> connect_stream( const OsSocket &,
                                      const net::NetAddress & ) noexcept override
    {
        return std::unexpected( net::NetError::NotInitialised );
    }
};

static fake_dll::State *state_of( sv::GameDll &dll )
{
    auto fn = reinterpret_cast<fake_dll::StateFn>( dll.symbol( "fake_state" ) );
    return fn ? fn() : nullptr;
}

struct ConnFixture
{
    xash::filesystem::Filesystem fs;
    cc::test::TrustedOracle      oracle;
    cc::test::NullPolicy         policy;
    cc::CmdCvarContext ctx = cc::test::make_test_context( oracle, policy );
    xash::MapLoader   maps;
    // Declared before `rt` so the NetworkContext (and its fragment pool)
    // outlives the per-client netchans that borrow it.
    FakeSockets         net_sockets;
    net::NetworkContext net_ctx;
    sv::ServerRuntime   rt;
    fake_dll::State  *st = nullptr;

    ConnFixture()
    {
        REQUIRE( fs.init( g_root.string(), "valve", "game" ) );
        fs.add_game_directory( ( g_root / "game" ).string(),
                               xash::filesystem::SearchPathFlags::GameDir );
        xash::MapLoaderInitParams mp;
        mp.filesystem = &fs;
        REQUIRE( maps.init( mp ) );

        // Bring the NetworkContext up so connect can arm each slot's netchan
        // (Netchan_Setup pulls the driver + fragment pool from here).
        REQUIRE( net_ctx.init( net::NetworkInitParams{ .sockets   = &net_sockets,
                                                       .dedicated = true } ) );
        // open the server socket so send_packet reaches the fake sendto.
        REQUIRE( net_ctx.config( /*multiplayer=*/true, /*change_port=*/false )
                     .has_value() );

        ( void )ctx.cvar_get_or_create( "sv_maxclients", "1", 0 );

        rt.cfg.game_dir   = "game";
        rt.cfg.game_dll   = FAKE_DLL_FULL;
        rt.cfg.max_edicts = 64;
        rt.cfg.dedicated  = false;
        rt.cvars          = &ctx;
        rt.fs             = &fs;
        rt.maps           = &maps;
        rt.net            = &net_ctx;

        REQUIRE( sv::spawn_server( rt, "conntest", nullptr, false ) );
        st = state_of( rt.game );
        REQUIRE( st != nullptr );
        rt.clients.realtime = 100.0;
    }

    ~ConnFixture()
    {
        sv::unload_progs( rt );
        maps.shutdown();
        fs.shutdown();
    }
};

static net::NetAddress client_adr()
{
    net::NetAddress a{};
    a.family     = net::IpFamily::V4;
    a.port       = 27005;
    a.addr.v4[0] = 1;
    a.addr.v4[1] = 2;
    a.addr.v4[2] = 3;
    a.addr.v4[3] = 4;
    return a;
}

// ---------------------------------------------------------------------------
// challenge is stateless + reproducible; current and previous window accepted.
// ---------------------------------------------------------------------------
static void test_challenge_roundtrip()
{
    ConnFixture fx;
    const net::NetAddress from = client_adr();

    const std::uint32_t window =
        static_cast<std::uint32_t>( fx.rt.clients.realtime / 5 );
    const std::int32_t chal =
        sv::compute_challenge( fx.rt.persistent.challenge_salt, from, window );

    CHECK( sv::check_challenge( fx.rt, from, chal ) );        // current window
    CHECK( !sv::check_challenge( fx.rt, from, chal ^ 0xFF ) ); // wrong value

    // loopback always hashes to 0.
    CHECK_EQ( sv::compute_challenge( fx.rt.persistent.challenge_salt,
                                     net::NetAddress::loopback_v4( 1 ), window ),
              0 );
}

// ---------------------------------------------------------------------------
// Full connect → new → spawn → begin walk drives every game client callback.
// ---------------------------------------------------------------------------
static void test_connect_and_enter_game()
{
    ConnFixture fx;
    CaptureSink sink;
    const net::NetAddress from = client_adr();

    // getchallenge → "challenge <N> 0"
    CHECK( sv::handle_connectionless( fx.rt, from, "getchallenge", sink ) );
    int chal = 0;
    CHECK_EQ( std::sscanf( sink.last, "challenge %d", &chal ), 1 );

    // connect
    char cmd[512];
    std::snprintf(
        cmd, sizeof( cmd ),
        "connect 49 %d "
        "\\qport\\27015\\ext\\1\\uuid\\0123456789abcdef0123456789abcdef "
        "\\name\\TestPlayer\\rate\\9999",
        chal );
    CHECK( sv::handle_connectionless( fx.rt, from, cmd, sink ) );

    sv::ServerClient &cl = fx.rt.clients.clients[0];
    CHECK( cl.state == sv::ClientState::Connected );
    CHECK_EQ( cl.userid, 1 );
    CHECK_EQ( cl.qport, static_cast<std::uint16_t>( 27015 ) );
    CHECK_EQ( cl.extensions, 1 ); // NET_EXT_SPLITSIZE granted
    CHECK_STREQ( cl.name, "TestPlayer" );
    CHECK( std::strncmp( sink.last, "client_connect", 14 ) == 0 );
    CHECK( fx.st->client_userinfo_calls >= 1 ); // pfnClientUserInfoChanged

    // new → pfnClientConnect
    sv::execute_client_command( fx.rt, cl, "new", sink );
    CHECK_EQ( fx.st->client_connect_calls, 1 );
    CHECK( cl.state == sv::ClientState::Connected );

    // spawn → pfnClientPutInServer, cs_spawning
    sv::execute_client_command( fx.rt, cl, "spawn", sink );
    CHECK_EQ( fx.st->client_put_in_server_calls, 1 );
    CHECK( cl.state == sv::ClientState::Spawning );

    // begin → cs_spawned
    sv::execute_client_command( fx.rt, cl, "begin", sink );
    CHECK( cl.state == sv::ClientState::Spawned );
    CHECK_EQ( cl.connecttime, 100.0 );

    // arbitrary stringcmd → forwarded to pfnClientCommand
    sv::execute_client_command( fx.rt, cl, "say hello", sink );
    CHECK_EQ( fx.st->client_command_calls, 1 );

    // drop (spawned) → pfnClientDisconnect + zombie, then timeout → free.
    sv::drop_client( fx.rt, cl, false );
    CHECK_EQ( fx.st->client_disconnect_calls, 1 );
    CHECK( cl.state == sv::ClientState::Zombie );
    sv::check_timeouts( fx.rt );
    CHECK( cl.state == sv::ClientState::Free );
}

// ---------------------------------------------------------------------------
// pfnClientConnect rejection drops the client back out.
// ---------------------------------------------------------------------------
static void test_connect_rejected_by_game()
{
    ConnFixture fx;
    CaptureSink sink;
    const net::NetAddress from = client_adr();

    fx.st->client_connect_should_reject = 1;

    const std::uint32_t window =
        static_cast<std::uint32_t>( fx.rt.clients.realtime / 5 );
    const std::int32_t chal =
        sv::compute_challenge( fx.rt.persistent.challenge_salt, from, window );

    const int slot = sv::connect_client(
        fx.rt, from, 49, chal,
        "\\qport\\27015\\uuid\\0123456789abcdef0123456789abcdef",
        "\\name\\Rejectee", sink );
    REQUIRE( slot == 0 );

    sv::ServerClient &cl = fx.rt.clients.clients[0];
    sv::execute_client_command( fx.rt, cl, "new", sink );
    CHECK_EQ( fx.st->client_connect_calls, 1 );
    CHECK( cl.state == sv::ClientState::Zombie ); // rejected ⇒ dropped
    // reject sends the three-packet SV_RejectConnection sequence (last is
    // "disconnect").
    CHECK( std::strncmp( sink.last, "disconnect", 10 ) == 0 );
}

// ---------------------------------------------------------------------------
// Bad protocol / bad challenge are rejected before slot allocation.
// ---------------------------------------------------------------------------
static void test_connect_validation()
{
    ConnFixture fx;
    CaptureSink sink;
    const net::NetAddress from = client_adr();

    CHECK_EQ( sv::connect_client( fx.rt, from, 48, 0, "\\uuid\\x", "\\name\\x",
                                  sink ),
              -1 ); // wrong protocol
    CHECK_EQ( sv::connect_client( fx.rt, from, 49, 12345, "\\uuid\\x",
                                  "\\name\\x", sink ),
              -1 ); // bogus challenge
    CHECK( fx.rt.clients.clients[0].state == sv::ClientState::Free );
}

// ---------------------------------------------------------------------------
// SV_FakeConnect: empty slot → spawned bot with FL_CLIENT|FL_FAKECLIENT.
// ---------------------------------------------------------------------------
static void test_fake_connect()
{
    ConnFixture fx;

    abi::edict_t *ed = sv::fake_connect( fx.rt, "Botty" );
    REQUIRE( ed != nullptr );

    sv::ServerClient &cl = fx.rt.clients.clients[0];
    CHECK( cl.state == sv::ClientState::Spawned );
    CHECK( cl.fakeclient );
    CHECK_STREQ( cl.name, "Botty" );
    CHECK( ( ed->v.flags & abi::k_fl_fakeclient ) != 0 );
    CHECK( ( ed->v.flags & abi::k_fl_client ) != 0 );

    // fake clients never time out.
    sv::check_timeouts( fx.rt );
    CHECK( cl.state == sv::ClientState::Spawned );
}

// ---------------------------------------------------------------------------
// Netchan lifecycle: connect arms the slot's channel through the host-owned
// NetworkContext (Netchan_Setup); drop flushes it (Netchan_Clear).
// ---------------------------------------------------------------------------
static void test_netchan_setup_and_clear()
{
    ConnFixture fx;
    CaptureSink sink;
    const net::NetAddress from = client_adr();

    const std::uint32_t window =
        static_cast<std::uint32_t>( fx.rt.clients.realtime / 5 );
    const std::int32_t chal =
        sv::compute_challenge( fx.rt.persistent.challenge_salt, from, window );

    const int slot = sv::connect_client(
        fx.rt, from, 49, chal,
        "\\qport\\27015\\uuid\\0123456789abcdef0123456789abcdef",
        "\\name\\NetchanPlayer", sink );
    REQUIRE( slot == 0 );

    net::Netchan &nc = fx.rt.clients.netchans[0];

    // Netchan_Setup ran against the injected NetworkContext: armed against the
    // peer, with the address + qport carried over from the connect command.
    CHECK( nc.is_active() );
    CHECK( nc.qport() == static_cast<std::uint16_t>( 27015 ) );
    CHECK( nc.remote_address() == from );

    // A queued reliable payload is flushed by Netchan_Clear at drop.
    const std::byte payload[4] = { std::byte{ 1 }, std::byte{ 2 },
                                   std::byte{ 3 }, std::byte{ 4 } };
    CHECK( nc.write_reliable( payload ) );
    CHECK( nc.reliable_length_bits() > 0 );

    sv::ServerClient &cl = fx.rt.clients.clients[0];
    sv::drop_client( fx.rt, cl, false );
    CHECK( nc.reliable_length_bits() == 0 );
}

// ---------------------------------------------------------------------------
// SV_ExecuteClientMessage: the clc_* opcode stream (delta / move / nop) updates
// delta_sequence, packet_loss, and lastcmd from a demuxed client message.
// ---------------------------------------------------------------------------
static void test_execute_client_message()
{
    ConnFixture fx;
    CaptureSink sink;
    const net::NetAddress from = client_adr();

    // Drive a client all the way to cs_spawned (netchan + frames ring armed).
    const std::uint32_t window =
        static_cast<std::uint32_t>( fx.rt.clients.realtime / 5 );
    const std::int32_t chal =
        sv::compute_challenge( fx.rt.persistent.challenge_salt, from, window );
    const int slot = sv::connect_client(
        fx.rt, from, 49, chal,
        "\\qport\\27015\\uuid\\0123456789abcdef0123456789abcdef",
        "\\name\\MsgPlayer", sink );
    REQUIRE( slot == 0 );

    sv::ServerClient &cl = fx.rt.clients.clients[0];
    sv::execute_client_command( fx.rt, cl, "new", sink );
    sv::execute_client_command( fx.rt, cl, "spawn", sink );
    sv::execute_client_command( fx.rt, cl, "begin", sink );
    REQUIRE( cl.state == sv::ClientState::Spawned );
    REQUIRE( cl.frames != nullptr );

    // Craft one client message: clc_delta(9), clc_move(loss=7, one usercmd),
    // clc_nop.  The move rides a real delta-usercmd written by the codec.
    std::array<std::byte, 256> buf{};
    net::MessageBuf            w{ std::span<std::byte>{ buf } };
    w.write_byte( static_cast<std::uint8_t>( sv::k_clc_delta ) );
    w.write_byte( 9 );

    w.write_byte( static_cast<std::uint8_t>( sv::k_clc_move ) );
    w.write_byte( 0 ); // checksum (skipped in this milestone)
    w.write_byte( 7 ); // packet_loss
    w.write_byte( 0 ); // numbackup
    w.write_byte( 1 ); // numcmds
    const abi::usercmd_t nullcmd = {};
    abi::usercmd_t       cmd      = {};
    cmd.msec        = 20;
    cmd.buttons     = 0x0004;
    cmd.forwardmove = 100.0f;
    fx.rt.delta.write_delta_usercmd( w, &nullcmd, &cmd );

    w.write_byte( static_cast<std::uint8_t>( sv::k_clc_nop ) );

    net::MessageBuf r;
    r.rebind_read(
        std::span<const std::byte>{ buf.data(), w.num_bytes_written() } );
    sv::execute_client_message( fx.rt, cl, r, sink );

    CHECK( cl.delta_sequence == 9 );               // clc_delta observed
    CHECK( cl.packet_loss == 7 );                  // clc_move header
    CHECK_EQ( static_cast<int>( cl.lastcmd.msec ), 20 );
    CHECK_EQ( static_cast<int>( cl.lastcmd.buttons ), 4 );
    CHECK( cl.state == sv::ClientState::Spawned ); // survived (no clc_bad drop)
}

// ---------------------------------------------------------------------------
// SV_SendClientMessages: a connecting client flagged for a reply transmits a
// keepalive packet through its netchan out to the NetworkContext (the empty,
// no-datagram-body path — no delta tables required).
// ---------------------------------------------------------------------------
static void test_send_client_keepalive()
{
    ConnFixture fx;
    CaptureSink sink;
    const net::NetAddress from = client_adr();

    const std::uint32_t window =
        static_cast<std::uint32_t>( fx.rt.clients.realtime / 5 );
    const std::int32_t chal =
        sv::compute_challenge( fx.rt.persistent.challenge_salt, from, window );
    const int slot = sv::connect_client(
        fx.rt, from, 49, chal,
        "\\qport\\27015\\uuid\\0123456789abcdef0123456789abcdef",
        "\\name\\SendPlayer", sink );
    REQUIRE( slot == 0 );

    // Still cs_connected (not spawned): SV_ReadPackets would flag a reply.
    sv::ServerClient &cl = fx.rt.clients.clients[0];
    cl.send_net_message = true;

    fx.net_sockets.sendto_calls = 0;
    sv::send_client_messages( fx.rt );

    // exactly one wire packet went out to the client, netchan advanced, flag
    // consumed.
    CHECK( fx.net_sockets.sendto_calls == 1 );
    CHECK( fx.net_sockets.last_dest == from );
    CHECK( fx.rt.clients.netchans[0].outgoing_sequence() > 1 );
    CHECK( !cl.send_net_message );
}

// ---------------------------------------------------------------------------
// SV_UpdateToReliableMessages: a staged reliable broadcast is fanned into every
// connected client's netchan reliable queue, then the broadcast buffer clears.
// ---------------------------------------------------------------------------
static void test_reliable_fanout()
{
    ConnFixture fx;
    CaptureSink sink;
    const net::NetAddress from = client_adr();

    const std::uint32_t window =
        static_cast<std::uint32_t>( fx.rt.clients.realtime / 5 );
    const std::int32_t chal =
        sv::compute_challenge( fx.rt.persistent.challenge_salt, from, window );
    const int slot = sv::connect_client(
        fx.rt, from, 49, chal,
        "\\qport\\27015\\uuid\\0123456789abcdef0123456789abcdef",
        "\\name\\RelPlayer", sink );
    REQUIRE( slot == 0 );

    // Stage a reliable broadcast (svc_lightstyle-shaped: two bytes).
    fx.rt.clients.reliable_datagram.reset();
    fx.rt.clients.reliable_datagram.write_byte( 12 ); // svc_lightstyle
    fx.rt.clients.reliable_datagram.write_byte( 0 );

    sv::update_to_reliable_messages( fx.rt );

    // The connected client's netchan reliable queue received the two bytes,
    // and the broadcast buffer was cleared.
    CHECK( fx.rt.clients.netchans[0].reliable_length_bits() >= 16 );
    CHECK( fx.rt.clients.reliable_datagram.num_bytes_written() == 0 );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    setup_tree();

    RUN_TEST( test_challenge_roundtrip );
    RUN_TEST( test_connect_and_enter_game );
    RUN_TEST( test_connect_rejected_by_game );
    RUN_TEST( test_connect_validation );
    RUN_TEST( test_fake_connect );
    RUN_TEST( test_netchan_setup_and_clear );
    RUN_TEST( test_execute_client_message );
    RUN_TEST( test_send_client_keepalive );
    RUN_TEST( test_reliable_fanout );

    std::filesystem::remove_all( g_root );
    std::printf( "server_client_state: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
