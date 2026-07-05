// xash3dpp — server physics driver + frame loop (Chunk 6 S8).
// Drives SV_Physics / Host_ServerFrame over the real fake game DLL + a real
// MapLoader loading the synthetic BSP.  Pins: the activate settle frames
// calling StartFrame + advancing framecount; SV_AddGravity on a toss entity;
// SV_RunThink dispatch; the SOLID_NOT pusher linear move; the dedicated
// one-step frame path; and the fixed-`sv_fps` zero-physics early-return quirk.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/physics.hpp>
#include <xash3dpp/private/server/world_links.hpp>

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
namespace ml  = xash::map_loader;
using Vec3    = xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

static std::filesystem::path g_root;

static const char *k_delta_lst =
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n"
    "}\n"
    // entity tables: SV_ActivateServer's SV_CreateBaseline signon-write serializes
    // every baseline through these (a real server's delta.lst always defines them).
    "entity_state_t none\n"
    "{\n"
    "    DEFINE_DELTA( origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
    "    DEFINE_DELTA( sequence, DT_INTEGER, 8, 1.0 )\n"
    "}\n"
    "entity_state_player_t none\n"
    "{\n"
    "    DEFINE_DELTA( origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
    "    DEFINE_DELTA( sequence, DT_INTEGER, 8, 1.0 )\n"
    "}\n";

static const char *k_spawn_entities =
    "{\n\"classname\" \"worldspawn\"\n}\n"
    "{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 0\"\n}\n";

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
    g_root = std::filesystem::temp_directory_path() / "xash3dpp_physics_test";
    std::filesystem::remove_all( g_root );
    std::filesystem::create_directories( g_root / "valve" );
    write_text( g_root / "game" / "delta.lst", k_delta_lst );

    auto builder = test_bsp::make_minimal_world();
    builder.set_entities( k_spawn_entities );
    write_bytes( g_root / "game" / "maps" / "phys.bsp", builder.build() );
}

static int  g_err_calls = 0;
static void err_hook( void *, const char * ) { ++g_err_calls; }

static fake_dll::State *state_of( sv::GameDll &dll )
{
    auto fn = reinterpret_cast<fake_dll::StateFn>( dll.symbol( "fake_state" ) );
    return fn ? fn() : nullptr;
}

// A ServerRuntime wired to a real MapLoader + CmdCvarContext, spawned +
// activated on the synthetic map, with the sv_* physics cvars registered.
struct PhysFixture
{
    xash::filesystem::Filesystem fs;
    cc::test::TrustedOracle      oracle;
    cc::test::NullPolicy         policy;
    cc::CmdCvarContext ctx = cc::test::make_test_context( oracle, policy );
    xash::MapLoader   maps;
    sv::ServerRuntime rt;
    fake_dll::State  *st = nullptr;

    PhysFixture( bool dedicated, int maxclients, float sv_fps )
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

        // SV_Init registers these with defaults; SV_UpdateMovevars reads them.
        (void)ctx.cvar_get_or_create( "sv_gravity", "800", 0 );
        (void)ctx.cvar_get_or_create( "sv_friction", "4", 0 );
        (void)ctx.cvar_get_or_create( "sv_stopspeed", "100", 0 );
        (void)ctx.cvar_get_or_create( "sv_maxvelocity", "2000", 0 );
        (void)ctx.cvar_get_or_create( "sv_stepsize", "18", 0 );
        if ( sv_fps != 0.0f )
        {
            char fbuf[16];
            std::snprintf( fbuf, sizeof( fbuf ), "%g", sv_fps );
            (void)ctx.cvar_get_or_create( "sv_fps", fbuf, 0 );
        }

        rt.cfg.game_dir   = "game";
        rt.cfg.game_dll   = FAKE_DLL_FULL;
        rt.cfg.max_edicts = 64;
        rt.cfg.dedicated  = dedicated;
        rt.cfg.host_error = err_hook;
        rt.cvars          = &ctx;
        rt.fs             = &fs;
        rt.maps           = &maps;
        g_err_calls       = 0;

        REQUIRE( sv::spawn_server( rt, "phys", nullptr, false ) );
        sv::spawn_entities( rt, *maps.world() );
        sv::activate_server( rt, /*run_physics=*/true );
        st = state_of( rt.game );
        REQUIRE( st != nullptr );
    }

    ~PhysFixture()
    {
        sv::unload_progs( rt );
        maps.shutdown();
        fs.shutdown();
    }

    // Allocate + init a live entity and link it into the world.
    abi::edict_t *spawn( int movetype, int solid, const Vec3 &origin,
                         const Vec3 &mins, const Vec3 &maxs )
    {
        abi::edict_t *e = rt.arena.alloc_edict( rt.level.time );
        REQUIRE( e != nullptr );
        rt.arena.init_edict( e );
        sv::EntityView v( e );
        v.set_movetype( movetype );
        v.set_solid( solid );
        v.set_origin( origin );
        v.set_mins( mins );
        v.set_maxs( maxs );
        rt.links.link_edict( e, false, rt.link_env );
        return e;
    }
};

// ---------------------------------------------------------------------------
// The settle frames run SV_Physics: StartFrame per frame + framecount.
// ---------------------------------------------------------------------------

static void test_settle_frames()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );

    // Single-player → 2 settle frames, each one full SV_Physics pass.
    CHECK_EQ( fx.st->start_frame_calls, 2 );
    CHECK_EQ( fx.rt.level.framecount, std::uint32_t{ 2 } );
    CHECK( fx.rt.level.state == sv::ServerState::Active );
    CHECK_EQ( g_err_calls, 0 );
}

// ---------------------------------------------------------------------------
// SV_AddGravity: a toss entity gains downward velocity + falls.
// ---------------------------------------------------------------------------

static void test_gravity()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );

    // Point toss entity (mins==maxs) — hull-0 traces never hit solid in the
    // synthetic world, so gravity + the fall are observable in isolation.
    abi::edict_t *ent = fx.spawn( abi::k_movetype_toss, abi::k_solid_not,
                                  Vec3{ 200.0f, 0.0f, 100.0f }, Vec3{}, Vec3{} );
    sv::EntityView v( ent );

    const float start_z = v.origin().z;
    const int   sf0     = fx.st->start_frame_calls;

    sv::sv_physics( fx.rt ); // rt.level.frametime == 0.1 (SP settle value)

    CHECK( v.velocity().z < 0.0f );         // gravity pulled it down
    CHECK( v.origin().z < start_z );        // and it moved down
    CHECK_EQ( fx.st->start_frame_calls, sf0 + 1 );
    CHECK_EQ( fx.rt.level.framecount, std::uint32_t{ 3 } );
    CHECK_EQ( g_err_calls, 0 );
}

// ---------------------------------------------------------------------------
// SV_RunThink: a due nextthink fires pfnThink and is cleared.
// ---------------------------------------------------------------------------

static void test_think()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );

    abi::edict_t *ent = fx.spawn( abi::k_movetype_none, abi::k_solid_not,
                                  Vec3{ 0.0f, 0.0f, 0.0f }, Vec3{}, Vec3{} );
    sv::EntityView v( ent );
    v.set_nextthink( 1.05f ); // due: sv.time(1.0) < 1.05 <= sv.time+frametime(1.1)

    const int th0 = fx.st->think_calls;
    sv::sv_physics( fx.rt );

    CHECK_EQ( fx.st->think_calls, th0 + 1 );
    CHECK_EQ( v.nextthink(), 0.0f ); // engine zeroes it before dispatch
    CHECK_EQ( g_err_calls, 0 );
}

// ---------------------------------------------------------------------------
// SV_Physics_Pusher: a SOLID_NOT mover advances linearly + rolls ltime.
// ---------------------------------------------------------------------------

static void test_pusher_linear_move()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );

    abi::edict_t *ent = fx.spawn( abi::k_movetype_push, abi::k_solid_not,
                                  Vec3{ 0.0f, 0.0f, 0.0f },
                                  Vec3{ -8.0f, -8.0f, -8.0f },
                                  Vec3{ 8.0f, 8.0f, 8.0f } );
    sv::EntityView v( ent );
    v.set_velocity( Vec3{ 0.0f, 0.0f, 10.0f } );
    v.set_nextthink( 100.0f ); // far future → a full-frametime move window

    sv::sv_physics( fx.rt );

    // velocity 10 * frametime 0.1 = +1.0 z; ltime advanced one frame.
    CHECK( v.origin().z > 0.9f && v.origin().z < 1.1f );
    CHECK( v.ltime() > 0.09f );
    CHECK_EQ( fx.st->blocked_calls, 0 );
    CHECK_EQ( g_err_calls, 0 );
}

// ---------------------------------------------------------------------------
// Host_ServerFrame (dedicated): one physics step per host frame + clock.
// ---------------------------------------------------------------------------

static void test_frame_loop_dedicated()
{
    PhysFixture fx( /*dedicated=*/true, /*maxclients=*/4, /*sv_fps=*/0.0f );

    const std::uint32_t fc0 = fx.rt.level.framecount; // 8 (MP settle)
    const double        t0  = fx.rt.level.time;

    sv::host_server_frame( fx.rt, 0.1 );

    CHECK_EQ( fx.rt.level.framecount, fc0 + 1 );        // exactly one step
    CHECK( fx.rt.level.time > t0 );                     // clock advanced
    CHECK( fx.rt.level.frametime > 0.09f && fx.rt.level.frametime < 0.11f );
    CHECK_EQ( g_err_calls, 0 );
}

// ---------------------------------------------------------------------------
// Host_ServerFrame fixed-`sv_fps`: the zero-physics-frames early-return quirk.
// ---------------------------------------------------------------------------

static void test_frame_loop_early_return()
{
    // Listen server (dedicated has no sv_fps) with sv_fps 20 → 1/19.99s steps.
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/20.0f );

    const std::uint32_t fc0 = fx.rt.level.framecount;

    // First host frame: residual is not accumulated yet (the simulating gate
    // is false on the first pass), so zero physics frames run → early return.
    sv::host_server_frame( fx.rt, 0.2 );
    CHECK_EQ( fx.rt.level.framecount, fc0 ); // nothing simulated (the quirk)

    // Second host frame: 0.2s of residual now drives several fixed steps.
    sv::host_server_frame( fx.rt, 0.2 );
    CHECK( fx.rt.level.framecount > fc0 );
    CHECK_EQ( g_err_calls, 0 );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    setup_tree();

    RUN_TEST( test_settle_frames );
    RUN_TEST( test_gravity );
    RUN_TEST( test_think );
    RUN_TEST( test_pusher_linear_move );
    RUN_TEST( test_frame_loop_dedicated );
    RUN_TEST( test_frame_loop_early_return );

    std::filesystem::remove_all( g_root );

    std::printf( "server_physics: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
