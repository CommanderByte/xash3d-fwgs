// xash3dpp — Chunk 6 milestone smoke: load the REAL retail Half-Life game DLL
// and run a single map frame.
//
// This is the dedicated-server deliverable's final tick: a real 32-bit
// valve/dlls/hl.dll is loaded through the production GameDll handshake, a real
// c0a0.bsp is loaded through the MapLoader, the full SV_SpawnServer →
// spawn_entities → SV_ActivateServer chain runs (driven by the MapLoader FSM
// via the ILevelChangeExecutor seam, exactly as the live engine drives it), and
// one Host_ServerFrame tick executes — all against our milestone engine table.
//
// Assets can't ship in-repo, so the harness is asset-gated and arch-gated:
//   • XASH_HL_ROOT must point at a Half-Life install (the folder holding
//     valve/); unset → SKIP.
//   • hl.dll is 32-bit, so a 64-bit process can't LoadLibrary it; on a non-32-
//     bit build → SKIP.
//   • valve/dlls/hl.dll or valve/maps/c0a0.bsp missing → SKIP.
// A SKIP is a pass (exit 0) so the suite stays green on CI and on machines
// without HL.  On an x86 build with XASH_HL_ROOT set, it exercises for real.
//
// Legacy reference: the engine's own boot path — Host_InitServer /
// SV_InitGame → SV_LoadProgs (sv_game.c:5214), then `map c0a0` →
// SV_SpawnServer (sv_init.c:935) → SV_ActivateServer (:579) → Host_Frame.

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/server/server.hpp>

#include "../../cmd_cvar/test_stubs.hpp"
#include "../../test_helpers.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace sv = xash::server;
namespace cc = xash::cmd_cvar;

static int g_pass = 0, g_fail = 0;

static int  g_err_calls = 0;
static char g_err_msg[512];

static void err_hook( void *, const char *msg )
{
    ++g_err_calls;
    std::snprintf( g_err_msg, sizeof( g_err_msg ), "%s", msg );
    std::printf( "  [host_error] %s\n", msg );
}

// ---------------------------------------------------------------------------
// The real-DLL smoke.  Returns true if it actually ran (asserts recorded via
// CHECK/REQUIRE), false if it skipped.
// ---------------------------------------------------------------------------

static bool run_smoke( const std::filesystem::path &root )
{
    namespace fsys = std::filesystem;

    const fsys::path dll = root / "valve" / "dlls" / "hl.dll";
    const fsys::path bsp = root / "valve" / "maps" / "c0a0.bsp";
    if ( !fsys::is_regular_file( dll ) || !fsys::is_regular_file( bsp ))
    {
        std::printf( "SKIP: %s or valve/maps/c0a0.bsp not found under %s\n",
                     dll.string().c_str(), root.string().c_str() );
        return false;
    }

    std::printf( "  HL root : %s\n", root.string().c_str() );
    std::printf( "  game DLL: %s\n", dll.string().c_str() );

    // Filesystem rooted at the HL install: base game AND current game are
    // valve (retail HL is its own mod).  add_game_directory mounts valve/ so
    // delta.lst / maps/ / the DLL search resolve.
    xash::filesystem::Filesystem fs;
    REQUIRE( fs.init( root.string(), "valve", "valve" ));
    fs.add_game_directory(( root / "valve" ).string(),
                          xash::filesystem::SearchPathFlags::GameDir );

    cc::test::TrustedOracle oracle;
    cc::test::NullPolicy    policy;
    cc::CmdCvarContext      ctx = cc::test::make_test_context( oracle, policy );
    (void)ctx.cvar_get_or_create( "sv_maxclients", "4", 0 );

    xash::MapLoader           maps;
    xash::MapLoaderInitParams mp;
    mp.filesystem = &fs;
    REQUIRE( maps.init( mp ));

    const std::string dll_path = dll.string();

    sv::Server           server;
    sv::ServerInitParams sp;
    sp.cvars      = &ctx;
    sp.fs         = &fs;
    sp.maps       = &maps;
    sp.game_dll   = dll_path.c_str();
    sp.game_dir   = "valve";
    sp.dedicated  = true;
    sp.max_edicts = 900; // GoldSrc default; c0a0 fits well under it
    sp.host_error = err_hook;
    REQUIRE( server.init( sp ));

    maps.set_level_executor( &server );
    g_err_calls = 0;

    // Drive the FSM exactly like the live engine's `map c0a0`: request the
    // level, then step it once.  The step delegates to Server::exec_load_level
    // → load_progs(real hl.dll) + spawn_server(c0a0) + spawn_entities +
    // activate_server.
    std::printf( "  loading level c0a0 (real hl.dll spawn)...\n" );
    maps.load_level( "c0a0", false );
    maps.run_frame_step();

    CHECK( server.initialized() );          // GiveFnptrsToDll + GameInit ran
    CHECK( server.active() );               // reached ss_active
    CHECK( maps.world() != nullptr );       // real BSP loaded
    CHECK( maps.state() == xash::MapLoadState::RunFrame );
    CHECK_EQ( g_err_calls, 0 );             // no Host_Error during spawn/activate

    if ( server.active())
    {
        // The milestone tick: one Host_ServerFrame (movevars → fixed-step
        // physics → prep-world-frame).  100ms of host wall-clock.
        std::printf( "  running one server frame...\n" );
        server.frame( 0.1 );
        CHECK_EQ( g_err_calls, 0 );         // frame ran clean
        std::printf( "  frame ran; server still active: %d\n",
                     static_cast<int>( server.active()));
    }

    server.shutdown();
    CHECK( !server.active() );

    maps.set_level_executor( nullptr );
    maps.shutdown();
    fs.shutdown();
    return true;
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    // Arch gate: hl.dll is 32-bit; a 64-bit process cannot load it.
    if constexpr ( sizeof( void * ) != 4 )
    {
        std::printf( "SKIP: hl.dll is 32-bit; this is a %zu-bit build "
                     "(build the x86 preset to run the real-DLL smoke)\n",
                     sizeof( void * ) * 8 );
        std::printf( "hl_smoke: skipped (arch)\n" );
        return 0;
    }

    // Asset gate: point at a Half-Life install via XASH_HL_ROOT.
    const char *root_env = std::getenv( "XASH_HL_ROOT" );
    if ( root_env == nullptr || root_env[0] == '\0' )
    {
        std::printf( "SKIP: set XASH_HL_ROOT to a Half-Life install "
                     "(the folder containing valve/) to run the real-DLL "
                     "smoke\n" );
        std::printf( "hl_smoke: skipped (no XASH_HL_ROOT)\n" );
        return 0;
    }

    const bool ran = run_smoke( std::filesystem::path( root_env ));
    if ( !ran )
    {
        std::printf( "hl_smoke: skipped (assets)\n" );
        return 0;
    }

    std::printf( "hl_smoke: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
