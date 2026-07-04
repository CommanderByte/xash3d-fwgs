// xash3dpp — level orchestration (Chunk 6 S7c1).
// Drives SV_SetupClients / SV_SpawnServer / SV_ActivateServer and the
// SV_DeactivateServer live path end-to-end against the real fake game DLL and
// a real MapLoader loading a synthetic BSP from a temp filesystem tree.  Pins:
// the maxclients latch + dedicated/listen clamp, the ss_loading→ss_active
// state walk, the world-model precache at slot WORLD_INDEX, the settle-frame
// count, pfnServerActivate's edict/client counts, the string-pool dynamic flip
// after activate, and the deactivate stale-world guard (globals nulled, env
// unbound, numEntities back to maxclients+1).

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/server/server.hpp>

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
namespace ml  = xash::map_loader;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Fixture tree: delta.lst (load_progs' Delta_Init) + the BSP on disk.
// ---------------------------------------------------------------------------

static std::filesystem::path g_root;

static const char *k_delta_lst =
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n"
    "}\n";

// worldspawn + one linkable entity (both LINK exports the fake DLL provides).
static const char *k_spawn_entities =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "\"message\" \"spawn test\"\n"
    "}\n"
    "{\n"
    "\"classname\" \"info_player_start\"\n"
    "\"origin\" \"0 0 0\"\n"
    "}\n";

static void write_text( const std::filesystem::path &p, const std::string &s )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( s.data(), static_cast<std::streamsize>( s.size() ));
}

static void write_bytes( const std::filesystem::path &p,
                         const std::vector<std::byte> &b )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( reinterpret_cast<const char *>( b.data() ),
             static_cast<std::streamsize>( b.size() ));
}

static void setup_tree()
{
    g_root = std::filesystem::temp_directory_path() / "xash3dpp_spawn_test";
    std::filesystem::remove_all( g_root );
    std::filesystem::create_directories( g_root / "valve" );
    write_text( g_root / "game" / "delta.lst", k_delta_lst );

    auto builder = test_bsp::make_minimal_world();
    builder.set_entities( k_spawn_entities );
    write_bytes( g_root / "game" / "maps" / "parsetest.bsp", builder.build() );
}

static int  g_err_calls = 0;
static char g_err_msg[256];

static void err_hook( void *, const char *msg )
{
    ++g_err_calls;
    std::snprintf( g_err_msg, sizeof( g_err_msg ), "%s", msg );
}

static fake_dll::State *state_of( sv::GameDll &dll )
{
    auto fn = reinterpret_cast<fake_dll::StateFn>( dll.symbol( "fake_state" ));
    return fn ? fn() : nullptr;
}

// ---------------------------------------------------------------------------
// Fixture: a ServerRuntime wired to a real MapLoader + CmdCvarContext.
// ---------------------------------------------------------------------------

struct SpawnFixture
{
    xash::filesystem::Filesystem fs;
    cc::test::TrustedOracle      oracle;
    cc::test::NullPolicy         policy;
    cc::CmdCvarContext           ctx = cc::test::make_test_context( oracle,
                                                                    policy );
    xash::MapLoader   maps;
    sv::ServerRuntime rt;
    fake_dll::State  *st = nullptr;

    SpawnFixture( bool dedicated, int maxclients )
    {
        REQUIRE( fs.init( g_root.string(), "valve", "game" ));
        fs.add_game_directory(( g_root / "game" ).string(),
                              xash::filesystem::SearchPathFlags::GameDir );

        xash::MapLoaderInitParams mp;
        mp.filesystem = &fs;
        REQUIRE( maps.init( mp ));

        char mcbuf[8];
        std::snprintf( mcbuf, sizeof( mcbuf ), "%d", maxclients );
        (void)ctx.cvar_get_or_create( "sv_maxclients", mcbuf, 0 );

        rt.cfg.game_dir   = "game";
        rt.cfg.game_dll   = FAKE_DLL_FULL;
        rt.cfg.max_edicts = 64;
        rt.cfg.dedicated  = dedicated;
        rt.cfg.host_error = err_hook;
        rt.cvars          = &ctx;
        rt.fs             = &fs;
        rt.maps           = &maps;

        g_err_calls = 0;
    }

    ~SpawnFixture()
    {
        sv::unload_progs( rt );
        maps.shutdown();
        fs.shutdown();
    }

    void refresh_state() { st = state_of( rt.game ); }
};

// ---------------------------------------------------------------------------
// SV_SetupClients: dedicated clamp latches maxclients to bound(4,·,MAX_CLIENTS)
// ---------------------------------------------------------------------------

static void test_dedicated_clamp()
{
    SpawnFixture fx( /*dedicated=*/true, /*maxclients=*/2 );

    REQUIRE( sv::spawn_server( fx.rt, "parsetest", nullptr, false ));

    // 2 is below the dedicated floor of 4.
    CHECK_EQ( fx.rt.persistent.maxclients, 4 );
    CHECK_EQ( fx.rt.arena.reserved(), std::size_t{ 5 } );
    CHECK_EQ( fx.rt.arena.num_entities(), std::size_t{ 5 } );
    CHECK_STREQ( fx.ctx.cvar_variable_string( "maxplayers" ), "4" );
    CHECK_EQ( fx.rt.globals.maxClients, 4 );
}

// ---------------------------------------------------------------------------
// Full walk: spawn (ss_loading) → spawn_entities → activate (ss_active) →
// deactivate (ss_dead + stale-world guard).
// ---------------------------------------------------------------------------

static void test_spawn_activate_deactivate()
{
    SpawnFixture fx( /*dedicated=*/false, /*maxclients=*/1 );

    REQUIRE( sv::spawn_server( fx.rt, "parsetest", nullptr, false ));
    fx.refresh_state();
    REQUIRE( fx.st != nullptr );

    // spawn_server ends at ss_loading with the world loaded + precached.
    CHECK( fx.rt.level.state == sv::ServerState::Loading );
    CHECK( fx.maps.world() != nullptr );
    CHECK_EQ( fx.rt.precache.model_index( "maps/parsetest.bsp" ),
              abi::k_world_index );
    CHECK( fx.rt.persistent.initialized );
    CHECK_EQ( fx.rt.persistent.maxclients, 1 );
    CHECK_EQ( fx.rt.arena.num_entities(), std::size_t{ 2 } ); // world + 1 client

    // Run the entity lump (exec_load_level chains this in S7c2).
    fx.st->spawn_calls = 0;
    sv::spawn_entities( fx.rt, *fx.maps.world() );
    CHECK_EQ( fx.st->spawn_calls, 2 ); // worldspawn + info_player_start
    CHECK( fx.rt.globals.mapname != 0 );

    // Activate: pfnServerActivate + settle frames + ss_active.
    // info_player_start alloc'd slot 2 → numEntities is now 3 (world + client
    // slot + ips); pfnServerActivate receives exactly svgame.numEntities.
    CHECK_EQ( fx.rt.arena.num_entities(), std::size_t{ 3 } );
    sv::activate_server( fx.rt, /*run_physics=*/true );
    CHECK( fx.rt.level.state == sv::ServerState::Active );
    CHECK_EQ( fx.st->server_activate_calls, 1 );
    CHECK_EQ( fx.st->activate_client_max, 1 );
    CHECK_EQ( fx.st->activate_edict_count,
              static_cast<int>( fx.rt.arena.num_entities() ));
    CHECK_STREQ( fx.ctx.cvar_variable_string( "host_serverstate" ), "2" );
    CHECK_EQ( g_err_calls, 0 );

    // Deactivate: pfnServerDeactivate, ss_dead, stale-world guard.
    sv::deactivate_server( fx.rt );
    CHECK( fx.rt.level.state == sv::ServerState::Dead );
    CHECK_EQ( fx.st->server_deactivate_calls, 1 );
    CHECK_EQ( fx.rt.globals.mapname, abi::string_t{ 0 } );
    CHECK_EQ( fx.rt.arena.num_entities(), std::size_t{ 2 } ); // maxclients + 1
    CHECK( fx.rt.bridge.move_env == nullptr );
    CHECK( fx.rt.bridge.links == nullptr );
    CHECK_STREQ( fx.ctx.cvar_variable_string( "host_serverstate" ), "0" );
}

// ---------------------------------------------------------------------------
// The save-restore activate path runs a single 0.001s frame (no crash, still
// reaches ss_active).
// ---------------------------------------------------------------------------

static void test_activate_no_physics()
{
    SpawnFixture fx( /*dedicated=*/false, /*maxclients=*/1 );

    REQUIRE( sv::spawn_server( fx.rt, "parsetest", nullptr, false ));
    sv::spawn_entities( fx.rt, *fx.maps.world() );
    sv::activate_server( fx.rt, /*run_physics=*/false );

    CHECK( fx.rt.level.state == sv::ServerState::Active );
    CHECK_EQ( fx.rt.level.frametime, 0.001f );
}

// ---------------------------------------------------------------------------
// Server as the MapLoader level-change executor: `map` drives the FSM, which
// delegates to exec_load_level → full spawn/activate.
// ---------------------------------------------------------------------------

static void test_level_executor()
{
    xash::filesystem::Filesystem fs;
    REQUIRE( fs.init( g_root.string(), "valve", "game" ));
    fs.add_game_directory(( g_root / "game" ).string(),
                          xash::filesystem::SearchPathFlags::GameDir );

    cc::test::TrustedOracle oracle;
    cc::test::NullPolicy    policy;
    cc::CmdCvarContext      ctx = cc::test::make_test_context( oracle, policy );
    (void)ctx.cvar_get_or_create( "sv_maxclients", "1", 0 );

    xash::MapLoader           maps;
    xash::MapLoaderInitParams mp;
    mp.filesystem = &fs;
    REQUIRE( maps.init( mp ));

    sv::Server            server;
    sv::ServerInitParams  sp;
    sp.cvars      = &ctx;
    sp.fs         = &fs;
    sp.maps       = &maps;
    sp.game_dll   = FAKE_DLL_FULL;
    sp.game_dir   = "game";
    sp.dedicated  = false;
    sp.max_edicts = 64;
    sp.host_error = err_hook;
    REQUIRE( server.init( sp ));

    maps.set_level_executor( &server );
    g_err_calls = 0;

    CHECK( !server.active() );
    CHECK( !server.initialized() );

    // Drive the FSM: request the level, then step it once.
    maps.load_level( "parsetest", false );
    maps.run_frame_step();

    CHECK( server.active() );
    CHECK( server.initialized() );
    CHECK( maps.world() != nullptr );
    CHECK( maps.state() == xash::MapLoadState::RunFrame );
    CHECK_EQ( g_err_calls, 0 );

    server.shutdown();
    CHECK( !server.active() );

    maps.set_level_executor( nullptr );
    maps.shutdown();
    fs.shutdown();
}

// No executor registered → the FSM keeps its inline Chunk-5 world load (the
// client background-map path + map_loader's own tests stay green).
static void test_no_executor_fallback()
{
    xash::filesystem::Filesystem fs;
    REQUIRE( fs.init( g_root.string(), "valve", "game" ));
    fs.add_game_directory(( g_root / "game" ).string(),
                          xash::filesystem::SearchPathFlags::GameDir );

    xash::MapLoader           maps;
    xash::MapLoaderInitParams mp;
    mp.filesystem = &fs;
    REQUIRE( maps.init( mp ));

    maps.load_level( "parsetest", false );
    maps.run_frame_step();

    CHECK( maps.world() != nullptr ); // inline load_world ran
    CHECK( maps.state() == xash::MapLoadState::RunFrame );

    maps.shutdown();
    fs.shutdown();
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    setup_tree();

    RUN_TEST( test_dedicated_clamp );
    RUN_TEST( test_spawn_activate_deactivate );
    RUN_TEST( test_activate_no_physics );
    RUN_TEST( test_level_executor );
    RUN_TEST( test_no_executor_fallback );

    std::filesystem::remove_all( g_root );

    std::printf( "server_spawn_server: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
