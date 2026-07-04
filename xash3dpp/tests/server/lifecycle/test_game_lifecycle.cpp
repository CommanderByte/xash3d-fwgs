// xash3dpp — game-DLL host lifecycle: load_progs / unload_progs (Chunk 6 S7)
// Legacy reference: engine/server/sv_game.c :5171-5366.
// End-to-end against the real fake-DLL double, a real Filesystem (temp
// game tree with delta.lst) and a real CmdCvarContext.  Pins: legacy load
// order effects (host_gameloaded, pStringBase, edict floor at maxclients+1
// with maxclients still 0, all-edicts-freed, GameInit + RegisterEncoders +
// hull bounds, Delta_Init), the unload dance (pfnGameShutdown sees the
// live cvar chain; chain + string allocs freed; bridge uninstalled), the
// persist-across-load early-return, and the Delta_Init failure unwind.

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>

#include "../abi/fake_dll_state.hpp"
#include "../../cmd_cvar/test_stubs.hpp"

#include "../../test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace cc  = xash::cmd_cvar;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Fixture tree: <tmp>/valve + <tmp>/game/delta.lst
// ---------------------------------------------------------------------------

static std::filesystem::path g_root;
static std::filesystem::path g_root_nodelta;

static const char *k_delta_lst =
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 ),\n"
    "    DEFINE_DELTA( ducking, DT_INTEGER, 1, 1.0 ),\n"
    "    DEFINE_DELTA( iparam1, DT_INTEGER | DT_SIGNED, 18, 1.0 )\n"
    "}\n";

static void write_file( const std::filesystem::path &p, const std::string &s )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( s.data(), static_cast<std::streamsize>( s.size() ));
}

static void setup_tree()
{
    g_root         = std::filesystem::temp_directory_path() / "xash3dpp_lifecycle_test";
    g_root_nodelta = std::filesystem::temp_directory_path() / "xash3dpp_lifecycle_nodelta";
    std::filesystem::remove_all( g_root );
    std::filesystem::remove_all( g_root_nodelta );

    std::filesystem::create_directories( g_root / "valve" );
    write_file( g_root / "game" / "delta.lst", k_delta_lst );

    std::filesystem::create_directories( g_root_nodelta / "valve" );
    std::filesystem::create_directories( g_root_nodelta / "game" );
}

static void init_fs( xash::filesystem::Filesystem &fs,
                     const std::filesystem::path &root )
{
    REQUIRE( fs.init( root.string(), "valve", "game" ));
    fs.add_game_directory(( root / "game" ).string(),
                          xash::filesystem::SearchPathFlags::GameDir );
}

// ---------------------------------------------------------------------------
// Host-error capture
// ---------------------------------------------------------------------------

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
// load → probe → unload
// ---------------------------------------------------------------------------

static void test_load_and_unload()
{
    xash::filesystem::Filesystem fs;
    init_fs( fs, g_root );

    cc::test::TrustedOracle oracle;
    cc::test::NullPolicy    policy;
    cc::CmdCvarContext ctx = cc::test::make_test_context( oracle, policy );

    sv::ServerRuntime rt;
    rt.cfg.game_dir       = "game";
    rt.cfg.max_edicts     = 64;
    rt.cfg.dedicated      = true;
    rt.cfg.host_error     = err_hook;
    rt.cvars              = &ctx;
    rt.fs                 = &fs;
    g_err_calls           = 0;

    REQUIRE( sv::load_progs( rt, FAKE_DLL_FULL ));
    CHECK( rt.game_loaded );
    CHECK( sv::engine_bridge() == &rt.bridge );

    // Legacy load-order effects.
    CHECK_STREQ( ctx.cvar_variable_string( "host_gameloaded" ), "1" );
    CHECK( rt.globals.pStringBase == rt.strings.base() );
    CHECK_EQ( rt.globals.maxEntities, 64 );
    CHECK_EQ( rt.globals.maxClients, 0 ); // svs.maxclients is 0 until SetupClients
    CHECK_EQ( rt.arena.num_entities(), std::size_t{ 1 } ); // world slot only
    CHECK( rt.arena.base() != nullptr );
    CHECK_EQ( rt.arena.base()[0].free, 1 );  // "mark all edicts as freed"
    CHECK_EQ( rt.arena.base()[63].free, 1 );

    fake_dll::State *st = state_of( rt.game );
    REQUIRE( st != nullptr );
    CHECK_EQ( st->game_init_calls, 1 );
    CHECK_EQ( st->register_encoders_calls, 1 );
    CHECK( st->engfuncs == &rt.engine_table );
    CHECK( st->globals == &rt.globals );

    // pfnGetHullBounds ×4 at the SV_InitClientMove position: hull 1 is
    // deliberately absent in the fake and must stay zeroed.
    CHECK_EQ( rt.hull_bounds[0].mins.z, -36.0f );
    CHECK_EQ( rt.hull_bounds[1].maxs.x, 0.0f );
    CHECK_EQ( rt.hull_bounds[3].mins.x, -32.0f );

    // Delta_Init ran against the fixture delta.lst.
    CHECK( rt.delta.is_initialized() );
    CHECK_EQ( rt.delta.table_field_count(
                  xash::networking::DeltaStructId::Event ), 3 );

    // Precache slots live through the installed bridge.
    CHECK_EQ( rt.engine_table.pfnPrecacheSound( "weapons/fire.wav" ), 1 );
    CHECK_EQ( rt.engine_table.pfnPrecacheSound( "weapons/fire.wav" ), 1 );
    CHECK_EQ( rt.engine_table.pfnModelIndex( "models/none.mdl" ), 0 );

    // Game-registered cvar chain + an engine-owned replacement string
    // (freed by the unload dance — the debug pool leak assert is the
    // real check here).
    static abi::cvar_t s_probe = { const_cast<char *>( "fake_shutdown_probe" ),
                                   const_cast<char *>( "0" ), 0, 0.0f,
                                   nullptr };
    rt.engine_table.pfnCVarRegister( &s_probe );
    CHECK( rt.bridge.external_cvars == &s_probe );
    rt.engine_table.pfnCVarSetString( "fake_shutdown_probe", "7" );
    CHECK_EQ( s_probe.value, 7.0f );

    // Persist-across-maps: second load is a no-op (no second handshake).
    CHECK( sv::load_progs( rt, FAKE_DLL_FULL ));
    CHECK_EQ( st->seq_len, 3 );
    CHECK_EQ( st->game_init_calls, 1 );

    // An entity that keeps DLL private data through unload must see
    // pfnOnFreeEntPrivateData while the DLL is still loaded (legacy
    // delivers it from the SV_FreeEdicts sweep; the arena teardown covers
    // it until S7b lands the deactivate body).  The fake increments
    // test-owned memory so the delivery is observable post-unload.
    abi::edict_t *holder = rt.arena.alloc_edict( 0.0 );
    REQUIRE( holder != nullptr );
    REQUIRE( rt.arena.alloc_private( holder, 32 ) != nullptr );
    int on_free_seen = 0;
    st->on_free_out  = &on_free_seen;

    // st dangles after this point.
    sv::unload_progs( rt );
    CHECK_EQ( on_free_seen, 1 );

    CHECK( !rt.game_loaded );
    CHECK( !rt.game.loaded() );
    CHECK( sv::engine_bridge() == nullptr );
    CHECK_STREQ( ctx.cvar_variable_string( "host_gameloaded" ), "0" );

    // The fake's pfnGameShutdown wrote 42 through pfnCVarSetFloat — proof
    // it ran AND ran while the cvar chain was still linked (the unlink
    // happens after, sv_game.c:5184-5202).  .string dangles by design;
    // only .value is readable now.
    CHECK_EQ( s_probe.value, 42.0f );

    CHECK_EQ( g_err_calls, 0 );

    // Idempotent.
    sv::unload_progs( rt );
    CHECK( !rt.game_loaded );

    fs.shutdown();
}

// ---------------------------------------------------------------------------
// Host_SetServerState mirror
// ---------------------------------------------------------------------------

static void test_set_server_state()
{
    cc::test::TrustedOracle oracle;
    cc::test::NullPolicy    policy;
    cc::CmdCvarContext ctx = cc::test::make_test_context( oracle, policy );

    sv::ServerRuntime rt;
    rt.cvars = &ctx;

    sv::set_server_state( rt, sv::ServerState::Loading );
    CHECK( rt.level.state == sv::ServerState::Loading );
    CHECK( rt.precache.loading() );
    CHECK_STREQ( ctx.cvar_variable_string( "host_serverstate" ), "1" );

    sv::set_server_state( rt, sv::ServerState::Active );
    CHECK( !rt.precache.loading() );
    CHECK_STREQ( ctx.cvar_variable_string( "host_serverstate" ), "2" );

    sv::set_server_state( rt, sv::ServerState::Dead );
    CHECK_STREQ( ctx.cvar_variable_string( "host_serverstate" ), "0" );
}

// ---------------------------------------------------------------------------
// Delta_Init failure unwinds deterministically (Q-5)
// ---------------------------------------------------------------------------

static void test_load_fails_without_delta()
{
    xash::filesystem::Filesystem fs;
    init_fs( fs, g_root_nodelta );

    cc::test::TrustedOracle oracle;
    cc::test::NullPolicy    policy;
    cc::CmdCvarContext ctx = cc::test::make_test_context( oracle, policy );

    sv::ServerRuntime rt;
    rt.cfg.max_edicts = 64;
    rt.cfg.host_error = err_hook;
    rt.cvars          = &ctx;
    rt.fs             = &fs;
    g_err_calls       = 0;

    CHECK( !sv::load_progs( rt, FAKE_DLL_FULL ));
    CHECK_EQ( g_err_calls, 1 );
    CHECK( std::strstr( g_err_msg, "Delta_Init" ) != nullptr );

    // Fully unwound: DLL gone, bridge detached, state flags reset.
    CHECK( !rt.game_loaded );
    CHECK( !rt.game.loaded() );
    CHECK( sv::engine_bridge() == nullptr );
    CHECK_STREQ( ctx.cvar_variable_string( "host_gameloaded" ), "0" );

    fs.shutdown();
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    setup_tree();

    RUN_TEST( test_load_and_unload );
    RUN_TEST( test_set_server_state );
    RUN_TEST( test_load_fails_without_delta );

    std::filesystem::remove_all( g_root );
    std::filesystem::remove_all( g_root_nodelta );

    std::printf( "server_game_lifecycle: %d passed, %d failed\n", g_pass,
                 g_fail );
    return g_fail == 0 ? 0 : 1;
}
