// xash3dpp — entity-string parse + SV_SetModel (Chunk 6 S7b).
// Drives SV_SpawnEntities/SV_LoadFromFile/SV_ParseEdict end-to-end against
// the real fake game DLL (LINK exports resolved by raw classname), a real
// Filesystem, a real CmdCvarContext, and a minimal BSP world.  Pins: the
// classname-first contract, the "wad"/empty skips, the '_' skysphere gate
// (negative case — flag clear), trailing-space stripping, the angle→angles
// rewrite, the custom-entity KeyValue path, pfnSpawn==-1 inhibition, the
// world-edict setup + origin/angles clear, and SV_SetModel brush-vs-studio
// bounds through the production IModelResolver.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/model_resolver.hpp>
#include <xash3dpp/private/server/world_hooks.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/world/trace.hpp>

#include "../abi/fake_dll_state.hpp"
#include "../../map_loader/bsp/test_bsp_builder.hpp"
#include "../../cmd_cvar/test_stubs.hpp"

#include "../../test_helpers.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace cc  = xash::cmd_cvar;
namespace ml  = xash::map_loader;
using ut_vec = xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Fixture tree (delta.lst so load_progs' Delta_Init succeeds).
// ---------------------------------------------------------------------------

static std::filesystem::path g_root;

static const char *k_delta_lst =
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n"
    "}\n";

static void write_file( const std::filesystem::path &p, const std::string &s )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( s.data(), static_cast<std::streamsize>( s.size() ));
}

static void setup_tree()
{
    g_root = std::filesystem::temp_directory_path() / "xash3dpp_parse_test";
    std::filesystem::remove_all( g_root );
    std::filesystem::create_directories( g_root / "valve" );
    write_file( g_root / "game" / "delta.lst", k_delta_lst );
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

// A worldspawn-first entity lump exercising every parse quirk.
static const char *k_entities =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "\"wad\" \"halflife.wad\"\n"          // skipped (already handled)
    "\"message\" \"Parse Test\"\n"
    "}\n"
    "{\n"
    "\"classname\" \"info_player_start\"\n"
    "\"origin\" \"16 32 48\"\n"
    "\"angle\" \"45\"\n"                   // → "angles" "0 45 0"
    "\"targetname \" \"spawn1\"\n"         // trailing space stripped
    "\"_comment\" \"util\"\n"              // NOT skipped (skysphere off)
    "}\n"
    "{\n"
    "\"classname\" \"trigger_reject\"\n"   // pfnSpawn == -1 → inhibited
    "\"targetname\" \"rej\"\n"
    "}\n"
    "{\n"
    "\"classname\" \"env_custom_thing\"\n" // no export → "custom" fallback
    "\"customfield\" \"7\"\n"
    "}\n";

// ---------------------------------------------------------------------------
// Fixture: loaded game DLL + world + installed world-interaction bridge.
// ---------------------------------------------------------------------------

struct ParseFixture
{
    xash::filesystem::Filesystem fs;
    cc::test::TrustedOracle      oracle;
    cc::test::NullPolicy         policy;
    cc::CmdCvarContext           ctx = cc::test::make_test_context( oracle,
                                                                    policy );
    sv::ServerRuntime            rt;

    std::optional<ml::WorldData> world;
    sv::ModelResolver            resolver;
    sv::WorldLinks               links;
    sv::GameWorldHooks           hooks;
    sv::MoveEnv                  env;
    sv::LinkEnv                  lenv;

    fake_dll::State *st = nullptr;

    ParseFixture( std::string_view entities, std::size_t max_edicts = 64 )
    {
        REQUIRE( fs.init( g_root.string(), "valve", "game" ));
        fs.add_game_directory(( g_root / "game" ).string(),
                              xash::filesystem::SearchPathFlags::GameDir );

        rt.cfg.game_dir   = "game";
        rt.cfg.max_edicts = max_edicts;
        rt.cfg.dedicated  = true;
        rt.cfg.host_error = err_hook;
        rt.cvars          = &ctx;
        rt.fs             = &fs;

        REQUIRE( sv::load_progs( rt, FAKE_DLL_FULL ));
        st = state_of( rt.game );
        REQUIRE( st != nullptr );

        // Build the world with the requested entity lump.
        auto builder = test_bsp::make_minimal_world();
        builder.set_entities( entities );
        ml::WorldLoadOptions opts;
        opts.is_world = true;
        auto w = ml::load_world_data( builder.build(), "parsetest", opts );
        REQUIRE( w.has_value() );
        world.emplace( std::move( *w ));

        // SV_SpawnServer precaches the world at slot 1 (mod_local.h:34).
        REQUIRE( rt.precache.model_index( "maps/parsetest.bsp" ) ==
                 abi::k_world_index );

        // Install the world-interaction bridge (S7c does this in
        // spawn_server; the parse/model paths need it live now).
        resolver.bind( &*world, &rt.precache, rt.fs );

        abi::edict_t *ws = rt.arena.edict_num( 0 );
        env.world      = &*world;
        env.models     = &resolver;
        env.area_root  = links.root();
        env.worldspawn = ws;

        lenv.world      = &*world;
        lenv.worldspawn = ws;

        hooks.bind( &rt.game, &env );
        links.set_hooks( &hooks );
        links.clear_world( { -4096, -4096, -4096 }, { 4096, 4096, 4096 } );

        rt.bridge.move_env = &env;
        rt.bridge.links    = &links;
        rt.bridge.link_env = &lenv;

        std::snprintf( rt.level.name, sizeof( rt.level.name ), "parsetest" );

        // Fresh probe counters (the module's g_state may persist).
        st->spawn_calls      = 0;
        st->set_abs_box_calls = 0;
        st->touch_calls      = 0;
        st->custom_link_calls = 0;
        st->kvd_len          = 0;
        g_err_calls          = 0;
    }

    ~ParseFixture()
    {
        sv::unload_progs( rt );
        fs.shutdown();
    }

    [[nodiscard]] bool has_kvd( const char *key, const char *val ) const
    {
        for ( int i = 0; i < st->kvd_len; ++i )
            if ( std::strcmp( st->kvds[i].key, key ) == 0 &&
                 ( val == nullptr ||
                   std::strcmp( st->kvds[i].val, val ) == 0 ))
                return true;
        return false;
    }
};

// ---------------------------------------------------------------------------
// SV_SpawnEntities + the ParseEdict quirk catalogue
// ---------------------------------------------------------------------------

static void test_spawn_and_parse()
{
    ParseFixture fx( k_entities );

    sv::spawn_entities( fx.rt, *fx.world );

    // World edict setup (sv_game.c:5154-5165) + the post-load origin clear.
    sv::EntityView w( fx.rt.arena.edict_num( 0 ));
    CHECK_EQ( w.modelindex(), abi::k_world_index );
    CHECK_EQ( w.solid(), abi::k_solid_bsp );
    CHECK_EQ( w.movetype(), abi::k_movetype_push );
    CHECK_EQ( w.origin().x, 0.0f );
    CHECK_EQ( w.angles().y, 0.0f );
    CHECK_STREQ( fx.rt.strings.get_string( w.classname()), "worldspawn" );
    CHECK( fx.rt.globals.mapname != 0 );
    CHECK_EQ( fx.rt.globals.maxEntities, 64 );

    // Four entities reach pfnSpawn (world, ips, reject, custom).
    CHECK_EQ( fx.st->spawn_calls, 4 );

    // Quirks.
    CHECK( fx.has_kvd( "angles", "0 45 0" ));  // angle → angles
    CHECK( fx.has_kvd( "targetname", nullptr )); // trailing space stripped
    CHECK( !fx.has_kvd( "targetname ", nullptr ));
    CHECK( fx.has_kvd( "_comment", nullptr ));   // '_' kept (skysphere off)
    CHECK( !fx.has_kvd( "wad", nullptr ));       // "wad" skipped
    CHECK( fx.has_kvd( "message", nullptr ));

    // Custom-entity path: fallback export + the customclass KeyValue.
    CHECK_EQ( fx.st->custom_link_calls, 1 );
    CHECK( fx.has_kvd( "customclass", "env_custom_thing" ));

    // trigger_reject was inhibited (freed); ips + custom stayed live.
    int  valid = 0;
    bool reject_live = false, ips_live = false, custom_live = false;
    for ( std::size_t i = 0; i < fx.rt.arena.num_entities(); ++i )
    {
        sv::EntityView v( fx.rt.arena.edict_num( i ));
        if ( !v.valid() )
            continue;
        ++valid;
        const char *cn = fx.rt.strings.get_string( v.classname());
        if ( std::strcmp( cn, "trigger_reject" ) == 0 )    reject_live = true;
        if ( std::strcmp( cn, "info_player_start" ) == 0 ) ips_live = true;
        if ( std::strcmp( cn, "env_custom_thing" ) == 0 )  custom_live = true;
    }
    CHECK_EQ( valid, 3 ); // world + ips + custom
    CHECK( !reject_live );
    CHECK( ips_live );
    CHECK( custom_live );
}

// ---------------------------------------------------------------------------
// SV_SetModel: brush submodel bounds vs studio/unknown zero bounds
// ---------------------------------------------------------------------------

static void test_set_model()
{
    ParseFixture fx( k_entities );

    // Brush model = the world (slot 1 → submodel 0): copies its bounds.
    abi::edict_t *brush = fx.rt.arena.alloc_edict( 1.0 );
    REQUIRE( brush != nullptr );
    fx.rt.engine_table.pfnSetModel( brush, "maps/parsetest.bsp" );

    sv::EntityView bv( brush );
    CHECK_EQ( bv.modelindex(), abi::k_world_index );
    const ml::SubModel &sm0 = fx.world->submodels()[0];
    CHECK_EQ( bv.mins().x, sm0.mins.x );
    CHECK_EQ( bv.maxs().z, sm0.maxs.z );
    // Relink ran through the game hook.
    CHECK( fx.st->set_abs_box_calls >= 1 );

    // Studio/unknown model: registered, but zero bounds (Chunk 7 loads it).
    abi::edict_t *studio = fx.rt.arena.alloc_edict( 1.0 );
    REQUIRE( studio != nullptr );
    fx.rt.engine_table.pfnSetModel( studio, "models/player.mdl" );

    sv::EntityView svw( studio );
    CHECK( svw.modelindex() > abi::k_world_index );
    CHECK_EQ( svw.size().x, 0.0f );
    CHECK_EQ( svw.mins().x, 0.0f );
    CHECK_EQ( svw.maxs().x, 0.0f );
}

// Legacy SV_AllocEdict Host_Errors on exhaustion and aborts the load; the
// port maps the arena's nullptr to the same host-error + abort.
static void test_arena_exhaustion()
{
    static const char *k_many =
        "{\n\"classname\" \"worldspawn\"\n}\n"
        "{\n\"classname\" \"info_player_start\"\n}\n"
        "{\n\"classname\" \"info_player_start\"\n}\n"
        "{\n\"classname\" \"info_player_start\"\n}\n";

    ParseFixture fx( k_many, 3 ); // world slot + 2 allocatable → 3rd alloc null
    sv::spawn_entities( fx.rt, *fx.world );

    CHECK( g_err_calls >= 1 );
    CHECK( std::strstr( g_err_msg, "no free edicts" ) != nullptr );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    setup_tree();

    RUN_TEST( test_spawn_and_parse );
    RUN_TEST( test_set_model );
    RUN_TEST( test_arena_exhaustion );

    std::filesystem::remove_all( g_root );

    std::printf( "server_entity_parse: %d passed, %d failed\n", g_pass,
                 g_fail );
    return g_fail == 0 ? 0 : 1;
}
