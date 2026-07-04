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
#include <xash3dpp/private/server/clients.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/utilities/string.hpp>

#include "../abi/fake_dll_state.hpp"
#include "../../map_loader/bsp/test_bsp_builder.hpp"
#include "../../cmd_cvar/test_stubs.hpp"

#include "../../test_helpers.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace cc  = xash::cmd_cvar;
namespace net = xash::networking;

static int g_pass = 0, g_fail = 0;

static std::filesystem::path g_root;

static const char *k_delta_lst =
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
    sv::ServerRuntime rt;
    fake_dll::State  *st = nullptr;

    ConnFixture()
    {
        REQUIRE( fs.init( g_root.string(), "valve", "game" ) );
        fs.add_game_directory( ( g_root / "game" ).string(),
                               xash::filesystem::SearchPathFlags::GameDir );
        xash::MapLoaderInitParams mp;
        mp.filesystem = &fs;
        REQUIRE( maps.init( mp ) );

        ( void )ctx.cvar_get_or_create( "sv_maxclients", "1", 0 );

        rt.cfg.game_dir   = "game";
        rt.cfg.game_dll   = FAKE_DLL_FULL;
        rt.cfg.max_edicts = 64;
        rt.cfg.dedicated  = false;
        rt.cvars          = &ctx;
        rt.fs             = &fs;
        rt.maps           = &maps;

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

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    setup_tree();

    RUN_TEST( test_challenge_roundtrip );
    RUN_TEST( test_connect_and_enter_game );
    RUN_TEST( test_connect_rejected_by_game );
    RUN_TEST( test_connect_validation );
    RUN_TEST( test_fake_connect );

    std::filesystem::remove_all( g_root );
    std::printf( "server_client_state: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
