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
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/map_loader/trace.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/physics.hpp>
#include <xash3dpp/private/server/pm_trace.hpp>
#include <xash3dpp/private/server/pmove.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/utilities/math.hpp>

#include "../abi/fake_dll_state.hpp"
#include "../../map_loader/bsp/test_bsp_builder.hpp"
#include "../../cmd_cvar/test_stubs.hpp"

#include "../../test_helpers.hpp"

#include <cstddef>
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

    // REGRESSION (modernization audit 2026-07-20): the ABI shim's sv.time
    // mirror must track the server clock. It was read at four pfn slots and
    // written nowhere in production, so it sat at 0.0 forever — which made
    // EdictArena's reuse guard (`freetime < 2.0f || sv_time - freetime > 0.5f`)
    // short-circuit TRUE on every game-DLL free, permanently disabling legacy's
    // 0.5 s slot-reuse grace (sv_game.c:1051). A stale mirror is invisible to
    // stub_scan and compliance_scan alike, so it needs a test.
    CHECK_EQ( fx.rt.bridge.sv_time, fx.rt.level.time );
    CHECK( fx.rt.bridge.sv_time > t0 );
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

    // The fixed-`sv_fps` loop is the OTHER clock-advance path; its sv.time
    // mirror must be stamped too (see the regression note above).
    CHECK_EQ( fx.rt.bridge.sv_time, fx.rt.level.time );
}

// ---------------------------------------------------------------------------
// pmove bridge (P2): SV_SetupPMove / SV_FinishPMove + the physent gather.
// ---------------------------------------------------------------------------

// A client-like edict linked into the world.  link_edict does not fill v.size
// (legacy SetMinMaxSize does), so set it explicitly — the gather treats a
// null-size entity as a point trigger and skips it.
static abi::edict_t *make_client( PhysFixture &fx, const Vec3 &origin )
{
    abi::edict_t *e = fx.spawn( abi::k_movetype_walk, abi::k_solid_bbox, origin,
                                Vec3{ -16, -16, -36 }, Vec3{ 16, 16, 36 } );
    e->v.flags |= abi::k_fl_client;
    e->v.health   = 100.0f;
    e->v.maxspeed = 320.0f;
    for ( int i = 0; i < 3; ++i )
        e->v.size[i] = e->v.maxs[i] - e->v.mins[i];
    return e;
}

// A solid modeled prop the gather should collect, with a deterministic absbox.
static abi::edict_t *spawn_prop( PhysFixture &fx, int solid, const Vec3 &origin,
                                 const Vec3 &mins, const Vec3 &maxs,
                                 int modelindex )
{
    abi::edict_t *e =
        fx.spawn( abi::k_movetype_none, solid, origin, mins, maxs );
    e->v.modelindex = modelindex;
    e->v.size[0] = maxs.x - mins.x;
    e->v.size[1] = maxs.y - mins.y;
    e->v.size[2] = maxs.z - mins.z;
    e->v.absmin[0] = origin.x + mins.x;
    e->v.absmin[1] = origin.y + mins.y;
    e->v.absmin[2] = origin.z + mins.z;
    e->v.absmax[0] = origin.x + maxs.x;
    e->v.absmax[1] = origin.y + maxs.y;
    e->v.absmax[2] = origin.z + maxs.z;
    return e;
}

static void test_pmove_setup_state()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 1;

    abi::edict_t *pl = make_client( fx, Vec3{ 100.0f, 0.0f, 0.0f } );
    pl->v.velocity[0] = 1.0f; pl->v.velocity[1] = 2.0f; pl->v.velocity[2] = 3.0f;
    pl->v.v_angle[0] = 10.0f; pl->v.v_angle[1] = 20.0f; pl->v.v_angle[2] = 30.0f;
    pl->v.teleport_time = 5.0f;

    sv::ServerClient cl;
    cl.edict    = pl;
    cl.timebase = 2.0;

    abi::usercmd_t ucmd{};
    ucmd.msec    = 50;
    ucmd.buttons = 0x0008;

    sv::sv_setup_pmove( fx.rt, cl, ucmd, "testphys" );
    const abi::playermove_t &pm = *fx.rt.pmove;

    CHECK_EQ( pm.player_index, fx.rt.arena.index_of( pl ) - 1 );
    CHECK_EQ( pm.multiplayer, 0 );
    CHECK( pm.time == 2000.0f ); // timebase(2.0) * 1000
    CHECK( pm.origin[0] == 100.0f && pm.origin[1] == 0.0f );
    CHECK( pm.velocity[0] == 1.0f && pm.velocity[2] == 3.0f );
    CHECK( pm.angles[0] == 10.0f && pm.angles[1] == 20.0f && pm.angles[2] == 30.0f );
    CHECK_EQ( pm.usehull, 0 );          // not ducking
    CHECK_EQ( pm.onground, 0 );         // SP → not forced to -1
    CHECK_EQ( pm.dead, 0 );             // health 100
    CHECK( pm.waterjumptime == 5.0f );  // teleport_time
    CHECK( pm.maxspeed == fx.rt.movevars.maxspeed );
    CHECK( pm.clientmaxspeed == 320.0f );
    CHECK_EQ( static_cast<int>( pm.cmd.msec ), 50 );
    CHECK_EQ( static_cast<int>( pm.cmd.buttons ), 0x0008 );
    CHECK( fx.rt.globals.frametime > 0.049f && fx.rt.globals.frametime < 0.051f );
    CHECK( std::string( pm.physinfo ) == "testphys" );
    CHECK( pm.numphysent >= 1 );        // world always present
    CHECK_EQ( g_err_calls, 0 );
}

static void test_pmove_setup_multiplayer_ducking()
{
    PhysFixture fx( /*dedicated=*/true, /*maxclients=*/4, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 4;

    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    pl->v.flags |= abi::k_fl_ducking;

    sv::ServerClient cl;
    cl.edict = pl;

    abi::usercmd_t ucmd{};
    ucmd.msec = 20;
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );
    const abi::playermove_t &pm = *fx.rt.pmove;

    CHECK_EQ( pm.multiplayer, 1 );
    CHECK_EQ( pm.onground, -1 ); // MP forces onground = -1
    CHECK_EQ( pm.usehull, 1 );   // FL_DUCKING
    CHECK_EQ( g_err_calls, 0 );
}

static void test_pmove_gather()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 1;

    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient cl;
    cl.edict = pl;
    abi::usercmd_t ucmd{};
    ucmd.msec = 50;

    // baseline: world only (the player has modelindex 0 → no physent model).
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );
    const int world_only = fx.rt.pmove->numphysent;
    CHECK_EQ( world_only, 1 );

    // a modeled prop within the 256-unit cube is gathered.
    const int mi = fx.rt.precache.model_index( "models/thing.mdl" );
    CHECK( mi > 0 );
    abi::edict_t *prop = spawn_prop( fx, abi::k_solid_bbox, Vec3{ 40, 0, 0 },
                                     Vec3{ -8, -8, -8 }, Vec3{ 8, 8, 8 }, mi );
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );
    CHECK_EQ( fx.rt.pmove->numphysent, 2 );
    CHECK_EQ( fx.rt.pmove->numvisent, 2 );
    // find the prop physent and check its copied fields.
    bool found = false;
    for ( int i = 0; i < fx.rt.pmove->numphysent; ++i )
    {
        const abi::physent_t &pe = fx.rt.pmove->physents[i];
        if ( pe.info == fx.rt.arena.index_of( prop ) )
        {
            found = true;
            CHECK_EQ( pe.solid, abi::k_solid_bbox );
            CHECK( pe.mins[0] == -8.0f && pe.maxs[2] == 8.0f );
            CHECK( std::string( pe.name ) == "models/thing.mdl" );
        }
    }
    CHECK( found );

    // moved beyond the cube → culled.
    prop->v.origin[0]  = 500.0f;
    prop->v.absmin[0]  = 492.0f;
    prop->v.absmax[0]  = 508.0f;
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );
    CHECK_EQ( fx.rt.pmove->numphysent, 1 ); // world only again

    // a model-less entity is skipped (legacy SV_ModelHandle NULL).
    (void)spawn_prop( fx, abi::k_solid_bbox, Vec3{ 30, 0, 0 },
                      Vec3{ -8, -8, -8 }, Vec3{ 8, 8, 8 }, /*modelindex=*/0 );
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );
    CHECK_EQ( fx.rt.pmove->numphysent, 1 );

    // a non-brush "ladder" is NOT a moveent (ladders require a brush model).
    abi::edict_t *fake_ladder = spawn_prop( fx, abi::k_solid_not, Vec3{ 20, 0, 0 },
                                            Vec3{ -8, -8, -8 }, Vec3{ 8, 8, 8 }, mi );
    fake_ladder->v.skin = ml::k_contents_ladder;
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );
    CHECK_EQ( fx.rt.pmove->nummoveent, 0 );
    CHECK_EQ( g_err_calls, 0 );
}

static void test_pmove_finish()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient cl;
    cl.edict = pl;

    abi::playermove_t &pm = *fx.rt.pmove;
    pm.origin[0] = 7.0f; pm.origin[1] = 8.0f; pm.origin[2] = 9.0f;
    pm.velocity[0] = 1.0f; pm.velocity[1] = 2.0f; pm.velocity[2] = 3.0f;
    pm.waterjumptime = 4.0f;
    pm.angles[0] = 30.0f; pm.angles[1] = 60.0f; pm.angles[2] = 12.0f;
    pm.usehull       = 1; // ducked hull
    pm.numphysent    = 1;
    pm.onground      = -1;
    pl->v.fixangle   = 0;
    pl->v.flags |= abi::k_fl_onground; // finish should clear it (onground -1)

    sv::sv_finish_pmove( fx.rt, cl );

    CHECK( pl->v.origin[0] == 7.0f && pl->v.origin[2] == 9.0f );
    CHECK( pl->v.velocity[1] == 2.0f );
    CHECK( pl->v.teleport_time == 4.0f );
    CHECK( ( pl->v.flags & abi::k_fl_onground ) == 0 );
    CHECK( pl->v.v_angle[0] == 30.0f && pl->v.v_angle[1] == 60.0f );
    CHECK( pl->v.angles[0] == -10.0f ); // pitch = -v_angle.pitch / 3
    CHECK( pl->v.angles[1] == 60.0f );  // yaw   = v_angle.yaw
    CHECK( pl->v.angles[2] == 12.0f );  // roll  = v_angle.roll
    // ducked hull extents (hull_bounds[1])
    CHECK( pl->v.mins[2] == fx.rt.hull_bounds[1].mins.z );
    CHECK( pl->v.maxs[2] == fx.rt.hull_bounds[1].maxs.z );
    CHECK( pl->v.size[2] == pl->v.maxs[2] - pl->v.mins[2] );

    // onground >= 0 resolves a groundentity from the physent info.
    pm.onground            = 0;
    pm.physents[0].info    = 0; // world
    sv::sv_finish_pmove( fx.rt, cl );
    CHECK( ( pl->v.flags & abi::k_fl_onground ) != 0 );
    CHECK( pl->v.groundentity == fx.rt.arena.edict_num( 0 ) );
    CHECK_EQ( g_err_calls, 0 );
}

// ---------------------------------------------------------------------------
// pmove trace family (P3a): the PM_* callbacks over the map_loader kernel.
// ---------------------------------------------------------------------------

static sv::PmTraceEnv make_pm_env( PhysFixture &fx )
{
    sv::PmTraceEnv env;
    env.world         = fx.rt.move_env.world;
    env.models        = &fx.rt.models;
    env.arena         = &fx.rt.arena;
    env.player_bounds = &fx.rt.hull_bounds;
    env.pusher_ext    = false;
    return env;
}

static void set_vec3( abi::vec3_t d, const Vec3 &v )
{
    d[0] = v.x;
    d[1] = v.y;
    d[2] = v.z;
}

// PM_TruePointContents / PM_PointContents over the world hull-0 (nodes):
// the minimal world's node0 splits +X at dist 128 into empty (front) / water.
static void test_pm_point_contents()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient cl;
    cl.edict = pl;
    abi::usercmd_t ucmd{};
    ucmd.msec = 50;
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );

    const sv::PmTraceEnv env = make_pm_env( fx );
    abi::playermove_t   &pm  = *fx.rt.pmove;

    // hull-0: X >= 128 → empty, X < 128 → water (unreferenced solid leaf).
    CHECK_EQ( sv::pm_true_point_contents( env, pm, Vec3{ 200.0f, 0.0f, 0.0f } ),
              ml::k_contents_empty );
    CHECK_EQ( sv::pm_true_point_contents( env, pm, Vec3{ 0.0f, 0.0f, 0.0f } ),
              ml::k_contents_water );
    // no water bmodels in the gather → point_contents == world base.
    CHECK_EQ( sv::pm_point_contents( env, pm, Vec3{ 0.0f, 0.0f, 0.0f } ),
              ml::k_contents_water );
    // CURRENT_* fold is a passthrough here (base is plain water, not a current).
    int truec = 0;
    CHECK_EQ(
        sv::pm_point_contents_pmove( env, pm, Vec3{ 0.0f, 0.0f, 0.0f }, &truec ),
        ml::k_contents_water );
    CHECK_EQ( truec, ml::k_contents_water );
    CHECK_EQ( g_err_calls, 0 );
}

// PM_PlayerTraceExt over the world physent must equal a direct map_loader
// hull_for_bsp + recursive_hull_check (the kernel it composes over) — this
// pins the offset, the non-rotated finalize, and the ent-index recording.
static void test_pm_player_trace_world()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient cl;
    cl.edict = pl;
    abi::usercmd_t ucmd{};
    ucmd.msec = 50;
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );

    const sv::PmTraceEnv env = make_pm_env( fx );
    abi::playermove_t   &pm  = *fx.rt.pmove;
    pm.usehull               = 0;

    const Vec3 start{ 200.0f, 0.0f, 0.0f };
    const Vec3 end{ -100.0f, 0.0f, 0.0f };

    // oracle: the raw kernel through the world's usehull-0 hull.
    const auto sel = ml::hull_for_bsp( *env.world, 0, 0, ( *env.player_bounds )[0],
                                       Vec3{ 0.0f, 0.0f, 0.0f } );
    ml::TraceResult exp{};
    exp.endpos = end;
    (void)ml::recursive_hull_check( sel.hull, sel.hull.firstclipnode, 0.0f, 1.0f,
                                    start - sel.offset, end - sel.offset, exp );
    if ( exp.allsolid )
        exp.startsolid = true;
    if ( exp.startsolid )
        exp.fraction = 0.0f;
    else
    {
        exp.endpos     = start + ( end - start ) * exp.fraction;
        exp.plane.dist = xash::utilities::dot( exp.endpos, exp.plane.normal );
    }

    const abi::pmtrace_t got = sv::pm_player_trace_ext(
        env, pm, start, end, 0, pm.physents, pm.numphysent, -1, nullptr );

    CHECK( got.fraction == exp.fraction );
    CHECK( got.endpos[0] == exp.endpos.x );
    CHECK( got.endpos[1] == exp.endpos.y );
    CHECK( got.endpos[2] == exp.endpos.z );
    CHECK( exp.fraction < 1.0f );        // the chosen ray exercises the hit path
    CHECK_EQ( got.ent, 0 );              // hit the world (physent 0)
    CHECK( got.plane.normal[0] == exp.plane.normal.x );
    CHECK( got.plane.dist == exp.plane.dist );
    CHECK_EQ( g_err_calls, 0 );
}

// A hand-built single SOLID_BBOX physent (isolated from the world) exercises
// the box-hull path, the ent-index return, PM_TraceLine list selection +
// usehull restore, and PM_TestPlayerPosition.
static void test_pm_box_physent()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    const int mi = fx.rt.precache.model_index( "models/thing.mdl" );
    CHECK( mi > 0 );
    abi::edict_t *prop = spawn_prop( fx, abi::k_solid_bbox, Vec3{ 40, 0, 0 },
                                     Vec3{ -8, -8, -8 }, Vec3{ 8, 8, 8 }, mi );

    const sv::PmTraceEnv env = make_pm_env( fx );
    abi::playermove_t   &pm  = *fx.rt.pmove;

    // isolate: one box physent (studio model → box path, no brush), no world.
    pm.usehull    = 0;
    pm.numphysent = 1;
    pm.physents[0] = abi::physent_t{};
    pm.physents[0].solid = abi::k_solid_bbox;
    set_vec3( pm.physents[0].origin, Vec3{ 40, 0, 0 } );
    set_vec3( pm.physents[0].mins, Vec3{ -8, -8, -8 } );
    set_vec3( pm.physents[0].maxs, Vec3{ 8, 8, 8 } );
    pm.physents[0].info = fx.rt.arena.index_of( prop );

    // sweep straight through the expanded box → a mid-ray impact on physent 0.
    const abi::pmtrace_t hit = sv::pm_player_trace_ext(
        env, pm, Vec3{ 100, 0, 0 }, Vec3{ 0, 0, 0 }, 0, pm.physents, 1, -1,
        nullptr );
    CHECK_EQ( hit.ent, 0 );
    CHECK( hit.fraction > 0.0f && hit.fraction < 1.0f );

    // ignore_pe skips the only ent → clear trace.
    const abi::pmtrace_t miss = sv::pm_player_trace_ext(
        env, pm, Vec3{ 100, 0, 0 }, Vec3{ 0, 0, 0 }, 0, pm.physents, 1,
        /*ignore_pe=*/0, nullptr );
    CHECK_EQ( miss.ent, -1 );
    CHECK( miss.fraction == 1.0f );

    // PM_STUDIO_IGNORE must NOT skip a non-studio bbox physent: legacy nests
    // the skip inside `if( pe->studiomodel )` (pm_trace.c:385-388), and
    // SV_CopyEdictToPhysEnt leaves studiomodel NULL when the model resolves
    // to no studio data — the entity is still bbox-traced under the flag.
    const abi::pmtrace_t still_hit = sv::pm_player_trace_ext(
        env, pm, Vec3{ 100, 0, 0 }, Vec3{ 0, 0, 0 },
        abi::k_pm_studio_ignore, pm.physents, 1, -1, nullptr );
    CHECK_EQ( still_hit.ent, 0 );
    CHECK( still_hit.fraction > 0.0f && still_hit.fraction < 1.0f );

    // PM_TraceLine: PHYSENTSONLY hits the box; ANYVISIBLE walks visents (empty).
    pm.numvisent = 0;
    const abi::pmtrace_t phys = sv::pm_trace_line(
        env, pm, Vec3{ 100, 0, 0 }, Vec3{ 0, 0, 0 },
        abi::k_pm_traceline_physentsonly, 0, -1 );
    const abi::pmtrace_t vis = sv::pm_trace_line(
        env, pm, Vec3{ 100, 0, 0 }, Vec3{ 0, 0, 0 },
        abi::k_pm_traceline_anyvisible, 0, -1 );
    CHECK( phys.fraction < 1.0f );
    CHECK( vis.fraction == 1.0f ); // no visents
    CHECK_EQ( pm.usehull, 0 );     // usehull restored after the swap

    // PM_TestPlayerPosition: inside the box → physent 0; far outside → -1.
    pm.origin[0] = 40.0f; // so the origin->origin probe is well-defined
    CHECK_EQ(
        sv::pm_test_player_position( env, pm, Vec3{ 40, 0, 0 }, nullptr, nullptr ),
        0 );
    CHECK_EQ( sv::pm_test_player_position( env, pm, Vec3{ 400, 0, 0 }, nullptr,
                                           nullptr ),
              -1 );
    CHECK_EQ( g_err_calls, 0 );
}

// PM_StuckTouch: dedup by ent, deltavelocity stamp, append, and the cap.
static void test_pm_stuck_touch()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    abi::playermove_t &pm = *fx.rt.pmove;

    pm.numtouch    = 0;
    pm.velocity[0] = 1.0f;
    pm.velocity[1] = 2.0f;
    pm.velocity[2] = 3.0f;

    abi::pmtrace_t tr{};
    sv::pm_stuck_touch( pm, 5, &tr );
    CHECK_EQ( pm.numtouch, 1 );
    CHECK_EQ( pm.touchindex[0].ent, 5 );
    CHECK( pm.touchindex[0].deltavelocity[0] == 1.0f );
    CHECK( pm.touchindex[0].deltavelocity[2] == 3.0f );

    sv::pm_stuck_touch( pm, 5, &tr ); // dedup: same ent not re-added
    CHECK_EQ( pm.numtouch, 1 );

    sv::pm_stuck_touch( pm, 6, &tr ); // a new ent appends
    CHECK_EQ( pm.numtouch, 2 );

    pm.numtouch = abi::k_max_physents; // at the cap → no append
    sv::pm_stuck_touch( pm, 7, &tr );
    CHECK_EQ( pm.numtouch, abi::k_max_physents );
    CHECK_EQ( g_err_calls, 0 );
}

// SV_InitClientMove (P3b): load_progs installs the PM_* callback table into
// rt.pmove; the collision slots are wired and reachable through the installed
// pointers, deferred slots are non-null stubs.
static void test_pm_init_client_move()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    abi::playermove_t &pm = *fx.rt.pmove;

    CHECK_EQ( static_cast<int>( pm.server ), 1 );
    CHECK( pm.movevars == &fx.rt.movevars );
    CHECK_EQ( static_cast<int>( pm.runfuncs ), 0 );
    // hull-bounds table copied in (pfnGetHullBounds enumeration).
    CHECK( pm.player_mins[0][2] == fx.rt.hull_bounds[0].mins.z );
    CHECK( pm.player_maxs[1][2] == fx.rt.hull_bounds[1].maxs.z );

    // collision-critical slots wired; deferred slots are non-null stubs.
    CHECK( pm.PM_PlayerTrace != nullptr );
    CHECK( pm.PM_PointContents != nullptr );
    CHECK( pm.PM_StuckTouch != nullptr );
    CHECK( pm.PM_TraceModel != nullptr );
    CHECK( pm.PM_PlaybackEventFull != nullptr );
    CHECK( pm.PM_TraceTexture != nullptr );
    CHECK( pm.PM_TraceSurface != nullptr );

    // functional call through the installed pointer: gather the world, then
    // PM_PointContents at X<128 → water (hull-0), reaching the bridge.
    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient cl;
    cl.edict = pl;
    abi::usercmd_t ucmd{};
    ucmd.msec = 50;
    sv::sv_setup_pmove( fx.rt, cl, ucmd, "" );

    float p[3]     = { 0.0f, 0.0f, 0.0f };
    int   truecont = 0;
    CHECK_EQ( pm.PM_PointContents( p, &truecont ), ml::k_contents_water );
    // group-c stub: no surface until Chunk 7.
    CHECK( pm.PM_TraceTexture( 0, p, p ) == nullptr );

    // PM_TraceModel must fill ONLY the shared trace_t/pmtrace_t prefix — the
    // engine trace_t is smaller, so a whole-struct copy would overflow the
    // caller's buffer (abi-watchdog).  Fill a pmtrace_t-sized buffer with a
    // sentinel and assert every byte past offsetof(ent) survives the call.
    alignas( abi::pmtrace_t ) unsigned char tbuf[sizeof( abi::pmtrace_t )];
    std::memset( tbuf, 0xAB, sizeof( tbuf ) );
    float ms[3] = { 200.0f, 0.0f, 0.0f };
    float me[3] = { 0.0f, 0.0f, 0.0f };
    (void)pm.PM_TraceModel( &pm.physents[0], ms, me,
                            reinterpret_cast<abi::trace_t *>( tbuf ) );
    for ( std::size_t i = offsetof( abi::pmtrace_t, ent ); i < sizeof( tbuf );
          ++i )
        CHECK_EQ( static_cast<int>( tbuf[i] ), 0xAB ); // untouched past prefix
    CHECK_EQ( g_err_calls, 0 );
}

// ---------------------------------------------------------------------------
// SV_RunCmd (P4): the per-usercmd player-move chain.
// ---------------------------------------------------------------------------

// The full chain fires in order, advances the timebase, passes the seed
// through, and SV_FinishPMove copies the PM_Move-advanced origin back.
static void test_sv_run_cmd_chain()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 1;
    fx.st->pm_move_dx = 8.0f; // PM_Move nudges origin[0] by +8

    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    const float   x0 = pl->v.origin[0];

    sv::ServerClient cl;
    cl.edict    = pl;
    cl.state    = sv::ClientState::Spawned;
    cl.timebase = 1.0;

    abi::usercmd_t ucmd{};
    ucmd.msec          = 20;
    ucmd.viewangles[1] = 45.0f;

    sv::sv_run_cmd( fx.rt, cl, ucmd, 0x1234 );

    CHECK_EQ( fx.st->cmd_start_calls, 1 );
    CHECK_EQ( fx.st->player_pre_think_calls, 1 );
    CHECK_EQ( fx.st->pm_move_calls, 1 );
    CHECK_EQ( fx.st->player_post_think_calls, 1 );
    CHECK_EQ( fx.st->cmd_end_calls, 1 );
    CHECK_EQ( static_cast<int>( fx.st->cmd_start_seed ), 0x1234 );
    CHECK_EQ( fx.st->pm_move_server, 1 ); // PM_Move( pmove, /*server=*/true )

    // timebase advanced by msec/1000; globals.frametime mirrors it.
    CHECK( fx.rt.globals.frametime > 0.0199f && fx.rt.globals.frametime < 0.0201f );
    CHECK( cl.timebase > 1.0199 && cl.timebase < 1.0201 );

    // viewangle latch (no fixangle) → v_angle follows the command.
    CHECK( pl->v.v_angle[1] == 45.0f );

    // SV_FinishPMove copied the PM_Move-advanced origin back onto the edict.
    CHECK( pl->v.origin[0] == x0 + 8.0f );
    CHECK_EQ( g_err_calls, 0 );
}

// msec > 50 splits into two half-length commands (impulse zeroed on the
// second half so it can't double-fire); each half runs the full chain.
static void test_sv_run_cmd_msec_split()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 1;

    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient cl;
    cl.edict    = pl;
    cl.state    = sv::ClientState::Spawned;
    cl.timebase = 0.0;

    abi::usercmd_t ucmd{};
    ucmd.msec    = 100; // > 50 → two 50 ms halves
    ucmd.impulse = 7;

    sv::sv_run_cmd( fx.rt, cl, ucmd, 0 );

    CHECK_EQ( fx.st->cmd_start_calls, 2 );
    CHECK_EQ( fx.st->pm_move_calls, 2 );
    CHECK_EQ( fx.st->cmd_end_calls, 2 );
    // timebase advanced by 2 × 50 ms.
    CHECK( cl.timebase > 0.0999 && cl.timebase < 0.1001 );
    // impulse was applied by the (first) half that carried it.
    CHECK_EQ( pl->v.impulse, 7 );
    CHECK_EQ( g_err_calls, 0 );
}

// A touch staged by PM_Move dispatches through SV_Impact (pfnTouch), and the
// client's real velocity is restored after the deltavelocity swap.
static void test_sv_run_cmd_touch()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients    = 1;
    fx.st->pm_move_inject_touch = 1;
    fx.st->pm_move_touch_ent    = 0; // world physent (physents[0].info == 0)

    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    pl->v.velocity[0] = 25.0f; // saved + restored around the touch loop

    sv::ServerClient cl;
    cl.edict = pl;
    cl.state = sv::ClientState::Spawned;

    abi::usercmd_t ucmd{};
    ucmd.msec = 20;

    const int touch0 = fx.st->touch_calls;
    sv::sv_run_cmd( fx.rt, cl, ucmd, 0 );

    // the injected touch reached SV_Impact → pfnTouch.
    CHECK( fx.st->touch_calls > touch0 );
    // velocity restored after the loop (deltavelocity was 111, not kept).
    CHECK( pl->v.velocity[0] == 25.0f );
    // numtouch reset for the next command.
    CHECK_EQ( fx.rt.pmove->numtouch, 0 );
    CHECK_EQ( g_err_calls, 0 );
}

// A kicked/zombie (or free) client is skipped entirely — no chain runs.
static void test_sv_run_cmd_zombie_skip()
{
    PhysFixture fx( /*dedicated=*/false, /*maxclients=*/1, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 1;

    abi::edict_t *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient cl;
    cl.edict = pl;
    cl.state = sv::ClientState::Zombie;

    abi::usercmd_t ucmd{};
    ucmd.msec = 20;
    sv::sv_run_cmd( fx.rt, cl, ucmd, 0 );

    CHECK_EQ( fx.st->cmd_start_calls, 0 );
    CHECK_EQ( fx.st->pm_move_calls, 0 );
    CHECK_EQ( g_err_calls, 0 );
}

// pfnRunPlayerMove (P4b): the game's fakeclient/bot mover — synthesize a cmd,
// set timebase, drive SV_RunCmd through the installed engine table (reaching
// bridge.runtime).  Only fakeclients are permitted.
static void test_run_player_move_fakeclient()
{
    PhysFixture fx( /*dedicated=*/true, /*maxclients=*/4, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 4;
    fx.st->pm_move_dx = 5.0f;
    fx.rt.level.time      = 3.0;
    fx.rt.level.frametime = 0.1f;

    abi::edict_t     *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient &cl = fx.rt.clients.clients[0];
    cl.state      = sv::ClientState::Spawned;
    cl.fakeclient = true;
    cl.edict      = pl;

    float va[3] = { 0.0f, 90.0f, 0.0f };
    fx.rt.engine_table.pfnRunPlayerMove( pl, va, 320.0f, 0.0f, 0.0f,
                                         /*buttons=*/0, /*impulse=*/0,
                                         /*msec=*/16 );

    CHECK_EQ( fx.st->cmd_start_calls, 1 );
    CHECK_EQ( fx.st->pm_move_calls, 1 );
    CHECK_EQ( static_cast<int>( cl.lastcmd.msec ), 16 );
    // timebase synthesised to land the command at time+frametime:
    // (3.0+0.1 - 0.016) + 0.016 == 3.1
    CHECK( cl.timebase > 3.0999 && cl.timebase < 3.1001 );
    CHECK( pl->v.origin[0] == 5.0f ); // FinishPMove copyback
    CHECK_EQ( g_err_calls, 0 );
}

// A non-fakeclient edict is rejected by pfnRunPlayerMove (real clients move via
// SV_ParseClientMove).
static void test_run_player_move_rejects_real_client()
{
    PhysFixture fx( /*dedicated=*/true, /*maxclients=*/4, /*sv_fps=*/0.0f );
    fx.rt.clients.maxclients = 4;

    abi::edict_t     *pl = make_client( fx, Vec3{ 0.0f, 0.0f, 0.0f } );
    sv::ServerClient &cl = fx.rt.clients.clients[0];
    cl.state      = sv::ClientState::Spawned;
    cl.fakeclient = false; // real client
    cl.edict      = pl;

    float va[3] = { 0.0f, 0.0f, 0.0f };
    fx.rt.engine_table.pfnRunPlayerMove( pl, va, 100.0f, 0.0f, 0.0f, 0, 0, 16 );

    CHECK_EQ( fx.st->pm_move_calls, 0 ); // rejected
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
    RUN_TEST( test_pmove_setup_state );
    RUN_TEST( test_pmove_setup_multiplayer_ducking );
    RUN_TEST( test_pmove_gather );
    RUN_TEST( test_pmove_finish );
    RUN_TEST( test_pm_point_contents );
    RUN_TEST( test_pm_player_trace_world );
    RUN_TEST( test_pm_box_physent );
    RUN_TEST( test_pm_stuck_touch );
    RUN_TEST( test_pm_init_client_move );
    RUN_TEST( test_sv_run_cmd_chain );
    RUN_TEST( test_sv_run_cmd_msec_split );
    RUN_TEST( test_sv_run_cmd_touch );
    RUN_TEST( test_sv_run_cmd_zombie_skip );
    RUN_TEST( test_run_player_move_fakeclient );
    RUN_TEST( test_run_player_move_rejects_real_client );

    std::filesystem::remove_all( g_root );

    std::printf( "server_physics: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
