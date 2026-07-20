// xash3dpp — save/restore server wiring (Chunk 8 S8.7).
// Drives the full in-process save->load round trip through the real fake game
// DLL: spawn a level, set a world field + sv.time, save_write_slot (SaveGameSlot
// -> .HL1/.HL2/.sav via the EntitySaver bridge + pfnSave), then save_exec_load_
// game (SV_LoadGame staging -> SpawnServer -> LoadGameState via the EntityRestorer
// bridge + pfnRestore -> ActivateServer(false)).  Pins: entity recreation
// (pfnRestore ran), the restored field value, sv.time application, and the
// save/load command registration + legacy privilege flags.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/command.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/save_bridge.hpp>

#include "../abi/fake_dll_state.hpp"
#include "../../map_loader/bsp/test_bsp_builder.hpp"
#include "../../cmd_cvar/test_stubs.hpp"

#include "../../test_helpers.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace cc  = xash::cmd_cvar;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Fixture tree (mirrors test_spawn_server): delta.lst + a synthetic BSP.
// ---------------------------------------------------------------------------

static std::filesystem::path g_root;

static const char *k_delta_lst =
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n"
    "}\n"
    "entity_state_t none\n"
    "{\n"
    "    DEFINE_DELTA( origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
    "    DEFINE_DELTA( sequence, DT_INTEGER, 8, 1.0 )\n"
    "}\n"
    "entity_state_player_t none\n"
    "{\n"
    "    DEFINE_DELTA( origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
    "    DEFINE_DELTA( sequence, DT_INTEGER, 8, 1.0 )\n"
    "}\n"
    "clientdata_t none\n"
    "{\n"
    "    DEFINE_DELTA( health, DT_SIGNED | DT_FLOAT, 16, 1.0 ),\n"
    "    DEFINE_DELTA( waterlevel, DT_INTEGER, 2, 1.0 )\n"
    "}\n"
    "weapon_data_t none\n"
    "{\n"
    "    DEFINE_DELTA( m_iClip, DT_SIGNED | DT_INTEGER, 10, 1.0 )\n"
    "}\n";

static const char *k_spawn_entities =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "\"message\" \"save test\"\n"
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

static void write_bytes( const std::filesystem::path &p, const std::vector<std::byte> &b )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( reinterpret_cast<const char *>( b.data() ),
             static_cast<std::streamsize>( b.size() ) );
}

static void setup_tree()
{
    g_root = std::filesystem::temp_directory_path() / "xash3dpp_save_test";
    std::filesystem::remove_all( g_root );
    std::filesystem::create_directories( g_root / "valve" );
    // The save/ scratch directory must exist before the filesystem write path
    // (write_file does not create parent dirs); the real engine creates it.
    std::filesystem::create_directories( g_root / "game" / "save" );
    write_text( g_root / "game" / "delta.lst", k_delta_lst );

    auto builder = test_bsp::make_minimal_world();
    builder.set_entities( k_spawn_entities );
    write_bytes( g_root / "game" / "maps" / "parsetest.bsp", builder.build() );
}

static int g_err_calls = 0;

static void err_hook( void *, const char *msg )
{
    ++g_err_calls;
    std::printf( "  [host_error] %s\n", msg );
}

static fake_dll::State *state_of( sv::GameDll &dll )
{
    auto fn = reinterpret_cast<fake_dll::StateFn>( dll.symbol( "fake_state" ) );
    return fn ? fn() : nullptr;
}

struct SpawnFixture
{
    xash::filesystem::Filesystem fs;
    cc::test::TrustedOracle      oracle;
    cc::test::NullPolicy         policy;
    cc::CmdCvarContext           ctx = cc::test::make_test_context( oracle, policy );
    xash::MapLoader              maps;
    sv::ServerRuntime            rt;
    fake_dll::State             *st = nullptr;

    SpawnFixture( bool dedicated, int maxclients )
    {
        REQUIRE( fs.init( g_root.string(), "valve", "game" ) );
        fs.add_game_directory( ( g_root / "game" ).string(),
                               xash::filesystem::SearchPathFlags::GameDir );

        xash::MapLoaderInitParams mp;
        mp.filesystem = &fs;
        REQUIRE( maps.init( mp ) );

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
        g_err_calls       = 0;
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
// Round trip: save the live level, then restore it into a fresh spawn.
// ---------------------------------------------------------------------------

static void test_save_load_roundtrip()
{
    // Listen server so maxclients can be 1 (the dedicated clamp forces >= 4,
    // which IsValidSave rejects as multiplayer).
    SpawnFixture fx( /*dedicated=*/false, /*maxclients=*/1 );

    REQUIRE( sv::spawn_server( fx.rt, "parsetest", nullptr, /*background=*/false ) );
    const xash::map_loader::WorldData *world = fx.maps.world();
    REQUIRE( world != nullptr );
    sv::spawn_entities( fx.rt, *world );
    sv::activate_server( fx.rt, /*run_physics=*/true );
    fx.refresh_state();
    REQUIRE( fx.st != nullptr );

    // A savegame precondition sanity: single-player + active => valid.
    CHECK( sv::save_is_valid( fx.rt ) );

    // Stamp a distinctive sv.time (the FIELD_TIME basis) + a field on every live
    // edict (so the last pfnSave's captured value is deterministic too).
    fx.rt.globals.time = 12.5f;
    for ( std::size_t i = 0; i < fx.rt.arena.num_entities(); ++i )
    {
        abi::edict_t *ed = fx.rt.arena.edict_num( i );
        if ( ed != nullptr && ed->free == 0 )
            ed->v.health = 777.0f;
    }
    abi::edict_t *world_ed = fx.rt.arena.edict_num( 0 );
    REQUIRE( world_ed != nullptr );

    // Parity divergences A/B/C: stamp skill, one sky cvar, and a lightstyle
    // BEFORE the save so the header/round trip has something distinctive to
    // carry (sv_save.c:1649-1658/996-1003).
    fx.rt.cvars->cvar_set( "skill", "2" );
    fx.rt.cvars->cvar_set( "sv_skyname", "testsky" );
    CHECK( fx.rt.lightstyles.set( 3, "aaaaz", 0.0f ) );

    const int saves_before = fx.st->save_calls;
    CHECK( sv::save_write_slot( fx.rt, "testsave", "roundtrip" ) );
    CHECK_LT( saves_before, fx.st->save_calls );        // pfnSave ran for live edicts
    CHECK_EQ( fx.st->saved_health, 777.0f );            // the world field was serialized

    // The .sav (+ the extracted .HL1/.HL2) now exist in the save dir.
    CHECK( fx.fs.file_exists( "save/testsave.sav", true ) );

    // Perturb the saved values so the restore assertions below can only pass
    // if the load path actually re-applies them (not just "still set").
    fx.rt.cvars->cvar_set( "skill", "0" );
    fx.rt.cvars->cvar_set( "sv_skyname", "bogus" );
    CHECK( fx.rt.lightstyles.set( 3, "m", 0.0f ) );

    // Restore: SV_LoadGame staging -> SpawnServer -> LoadGameState -> activate(false).
    const int restores_before = fx.st->restore_calls;
    CHECK( sv::save_exec_load_game( fx.rt, "testsave" ) );
    CHECK( fx.rt.level.state == sv::ServerState::Active );

    // Entity recreation: pfnRestore ran for the recreated edicts.
    CHECK_LT( restores_before, fx.st->restore_calls );

    // The world field survived the round trip (recreated edict 0, restored value).
    abi::edict_t *world2 = fx.rt.arena.edict_num( 0 );
    REQUIRE( world2 != nullptr );
    CHECK_EQ( world2->v.health, 777.0f );

    // sv.time = header.time, applied after SpawnServer reset it to the spawn epoch.
    CHECK( fx.rt.level.time >= 12.49 && fx.rt.level.time <= 12.51 );

    // The restore left the server paused/loadgame until a client connects.
    CHECK( fx.rt.level.loadgame );
    CHECK( fx.rt.level.paused );

    // Divergence A (skill) + B (one sky cvar): re-applied from the loaded
    // header (sv_save.c:1649-1650), not left at the perturbed value.
    CHECK_STREQ( fx.rt.cvars->cvar_variable_string( "skill" ), "2" );
    CHECK_STREQ( fx.rt.cvars->cvar_variable_string( "sv_skyname" ), "testsky" );

    // Divergence C (lightstyles): reset-all-then-apply restored the saved
    // pattern, not the perturbed one (sv_save.c:996-1003).
    const sv::LightStyle *ls = fx.rt.lightstyles.style( 3 );
    REQUIRE( ls != nullptr );
    CHECK_STREQ( ls->pattern, "aaaaz" );

    // BLOCKER (szCurrentMapName, eiface.h:345): the fake DLL's last pfnRestore
    // observed the LEVEL being loaded ("parsetest" — the .sav container's
    // GAME_HEADER.mapName), not the save-FILE name ("testsave") and not an
    // empty/stale buffer.
    CHECK_STREQ( fx.st->restore_map_name, "parsetest" );

    // Deviation E (changelevel-flag lifetime) witness: NOT exercised here —
    // this fixture only ever spawns a single map ("parsetest"), and
    // save_exec_change_level (the only path that ever sets
    // globals->changelevel true) requires a SECOND map to change into.
    // Standing up a second BSP/delta.lst fixture is out of scope for this
    // pass; the fix (the save_bridge.cpp early `changelevel = 0` before
    // activate_server was removed, so only activate_server's sv_init.c:645-
    // position clear fires) is verified by code inspection instead — no test
    // in this binary drives save_exec_change_level.
}

// ---------------------------------------------------------------------------
// Deviation D: the aged-slot rotation budget is 2 (SAVE_AGED_COUNT,
// filesystem.c:53/788), not the old wrong-comment default of 1.  Saving
// "quick" three times in a row should retain TWO aged slots (quick01 +
// quick02) alongside the current quick.sav — with the old (wrong) budget of
// 1, quick02 would never accumulate (AgeSaveList's delete step would evict
// quick01 every rotation instead of shifting it down first).
// ---------------------------------------------------------------------------

static void test_aged_count_rotation()
{
    SpawnFixture fx( /*dedicated=*/false, /*maxclients=*/1 );

    REQUIRE( sv::spawn_server( fx.rt, "parsetest", nullptr, /*background=*/false ) );
    const xash::map_loader::WorldData *world = fx.maps.world();
    REQUIRE( world != nullptr );
    sv::spawn_entities( fx.rt, *world );
    sv::activate_server( fx.rt, /*run_physics=*/true );

    CHECK( sv::save_write_slot( fx.rt, "quick", "1" ) );
    CHECK( sv::save_write_slot( fx.rt, "quick", "2" ) );
    CHECK( sv::save_write_slot( fx.rt, "quick", "3" ) );

    CHECK( fx.fs.file_exists( "save/quick.sav", true ) );
    CHECK( fx.fs.file_exists( "save/quick01.sav", true ) );
    // Only reachable with an aged-count budget of 2 — the bug's default of 1
    // would evict quick01 every rotation before a quick02 could accumulate.
    CHECK( fx.fs.file_exists( "save/quick02.sav", true ) );
}

// ---------------------------------------------------------------------------
// Command registration: names present + the legacy privilege split.
// ---------------------------------------------------------------------------

static void test_save_command_registration()
{
    cc::test::TrustedOracle oracle;
    cc::test::NullPolicy    policy;
    cc::CmdCvarContext      ctx = cc::test::make_test_context( oracle, policy );

    sv::ServerRuntime      rt;
    sv::SaveCommandContext cmd_ctx;
    cmd_ctx.rt = &rt;
    sv::register_save_commands( ctx, cmd_ctx );

    for ( const char *name :
          { "save", "load", "savequick", "loadquick", "autosave", "killsave", "reload" } )
        CHECK( ctx.cmd_exists( name ) );

    // Cmd_AddRestrictedCommand => FCMD_PRIVILEGED (load/loadquick/reload/killsave).
    CHECK( ( ctx.cmd_describe( "load" ).flags & cc::FCMD_PRIVILEGED ) != 0 );
    CHECK( ( ctx.cmd_describe( "killsave" ).flags & cc::FCMD_PRIVILEGED ) != 0 );
    // Cmd_AddCommand => not privileged (save/savequick/autosave).
    CHECK( ( ctx.cmd_describe( "save" ).flags & cc::FCMD_PRIVILEGED ) == 0 );
    CHECK( ( ctx.cmd_describe( "autosave" ).flags & cc::FCMD_PRIVILEGED ) == 0 );

    sv::unregister_save_commands( ctx );
    CHECK( !ctx.cmd_exists( "save" ) );
    CHECK( !ctx.cmd_exists( "load" ) );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    setup_tree();

    RUN_TEST( test_save_load_roundtrip );
    RUN_TEST( test_aged_count_rotation );
    RUN_TEST( test_save_command_registration );

    std::printf( "test_save_restore: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
