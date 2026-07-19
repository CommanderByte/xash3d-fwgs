// xash3dpp — level orchestration: SV_SetupClients / SV_SpawnServer /
// SV_ActivateServer (Chunk 6 S7c).
// Legacy reference: engine/server/sv_init.c — SV_SetupClients (:790-834),
// SV_SpawnServer (:935-1078), SV_ActivateServer (:579-673),
// SV_FreeOldEntities (:548-570).
// Deep dive: docs/legacy-survey/deep-dive-server-lifecycle.md §5-6.
//
// These are the free functions that turn a loaded game DLL + a map name into
// a live, activated server.  spawn_server drives the world load back through
// MapLoader (world ownership stays there — Q-6) and installs the world-
// interaction bridge the trace/link kernels and the ABI shim reach.  The
// entity lump is NOT run here: exec_load_level runs spawn_server then
// spawn_entities then activate_server (the seam lands in S7c2).
//
// Milestone trims (all XASH3DPP-STUB-marked) hold the legacy ORDER points so
// later slices only fill bodies: the snapshot ring / client array / netchan /
// baselines / testpacket (S9), the SV_Physics settle-frame body + movevars
// (S8), the server log + challenge salt + timestart clock (S9).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/lifecycle.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/physics.hpp>
#include <xash3dpp/private/server/world_links.hpp>

#include <cstdio>
#include <cstring>

namespace xash::server {

namespace {

namespace ml = ::xash::map_loader;

// MAX_CLIENTS (protocol.h:104, 1 << MAX_CLIENT_BITS = 32) is provided by
// clients.hpp as xash::server::k_max_clients (pulled in via lifecycle.hpp).

// server.h:50 — SV_SPAWN_TIME (the settle-frame frametime).
constexpr float k_sv_spawn_time = 0.1f;

void host_error( ServerRuntime &rt, const char *msg ) noexcept
{
    if ( rt.cfg.host_error != nullptr )
        rt.cfg.host_error( rt.cfg.host_error_ctx, msg );
    else
        ::xash::core::log_error( "server", msg );
}

[[nodiscard]] int clamp_int( int lo, int v, int hi ) noexcept
{
    return v < lo ? lo : ( v > hi ? hi : v );
}

// COM_StripExtension (public/crtlib.c): truncate at the last '.' (legacy
// scans backward for the first '.' — i.e. the last one — with no path-
// separator check; map names carry no directory here so the quirk is inert).
void strip_extension( char *path ) noexcept
{
    int last_dot = -1;
    for ( int i = 0; path[i] != '\0'; ++i )
        if ( path[i] == '.' )
            last_dot = i;
    if ( last_dot >= 0 )
        path[last_dot] = '\0';
}

// SV_ClearWorld + the model/link environment (the fixture block from the S7b
// parse test, now owned by the runtime): rebind the resolver + the MoveEnv/
// LinkEnv over the freshly loaded world, then rebuild the areanode tree.
void install_world_bridge( ServerRuntime &rt, const ml::WorldData &world ) noexcept
{
    ::xash::abi::edict_t *ws = rt.arena.edict_num( 0 );

    rt.models.bind( &world, &rt.precache, rt.fs );

    rt.move_env            = MoveEnv{};
    rt.move_env.world      = &world;
    rt.move_env.models     = &rt.models;
    rt.move_env.area_root  = rt.links.root();
    rt.move_env.worldspawn = ws;
    // OQ-2 studio hull gating inputs: live cvar reads (sv_clienttrace,
    // mod_studiocache) + the game-set per-call trace flags
    // (svgame.globals->trace_flags; FTRACE_SIMPLEBOX).
    rt.move_env.cvars       = rt.cvars;
    rt.move_env.trace_flags = &rt.globals.trace_flags;

    rt.link_env            = LinkEnv{};
    rt.link_env.world      = &world;
    rt.link_env.worldspawn = ws;

    rt.hooks.bind( &rt.game, &rt.move_env );
    rt.links.set_hooks( &rt.hooks );

    // SV_ClearWorld areanode part — the tree spans the world submodel bounds.
    const ml::SubModel &w0 = world.submodels()[0];
    rt.links.clear_world( w0.mins, w0.maxs );

    // SV_ClearWorld also resets every lightstyle to full (sv_world.c:473-477);
    // wire the bridge so pfnLightStyle writes here and SV_RunLightStyles reads.
    rt.lightstyles.reset();
    rt.bridge.lightstyles = &rt.lightstyles;

    rt.bridge.move_env = &rt.move_env;
    rt.bridge.links    = &rt.links;
    rt.bridge.link_env = &rt.link_env;
}

// SV_FreeOldEntities (sv_init.c:548-570): free FL_KILLME entities above the
// client slots, then trim numEntities past any trailing free slots.
void free_old_entities( ServerRuntime &rt ) noexcept
{
    const std::size_t start =
        static_cast<std::size_t>( rt.persistent.maxclients ) + 1;
    for ( std::size_t i = start; i < rt.arena.num_entities(); ++i )
    {
        ::xash::abi::edict_t *ent = rt.arena.edict_num( i );
        EntityView v( ent );
        if ( !v.freed() &&
             ( v.flags() & ::xash::abi::k_fl_killme ) != 0 )
        {
            WorldLinks::unlink_edict( ent ); // SV_FreeEdict unlinks first
            rt.arena.free_edict( ent, rt.level.time );
        }
    }

    while ( rt.arena.num_entities() > 0 )
    {
        ::xash::abi::edict_t *top =
            rt.arena.edict_num( rt.arena.num_entities() - 1 );
        if ( top == nullptr || !EntityView( top ).freed() )
            break;
        rt.arena.set_num_entities( rt.arena.num_entities() - 1 );
    }
}

} // namespace

void setup_clients( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // Pre-S7 fixtures without a cvar context keep the load-time floor
    // (maxclients 0 → world-only), exactly like a game DLL loaded at engine
    // start before any spawn.
    if ( rt.cvars == nullptr )
        return;

    const int desired =
        static_cast<int>( rt.cvars->cvar_variable_value( "sv_maxclients" ));

    if ( rt.persistent.maxclients == desired )
        return; // nothing to change

    // Legacy runs a full SV_Shutdown when maxclients changes on a live
    // server.  XASH3DPP-STUB(chunk6-S9): the Server-level teardown (client
    // array / server log / master heartbeat) lands with the client
    // machinery; the first-spawn path (maxclients 0 → N) never reaches it.

    int mc = desired;
    if ( rt.cfg.dedicated )
        mc = clamp_int( 4, mc, k_max_clients );
    else
        mc = clamp_int( 1, mc, k_max_clients );
    rt.persistent.maxclients = mc;

    // Make the gamemode cvars consistent (the globals are stamped in
    // spawn_server from these values).
    rt.cvars->cvar_set( "deathmatch", mc == 1 ? "0" : "1" );
    if ( rt.cvars->cvar_variable_value( "coop" ) != 0.0f )
        rt.cvars->cvar_set( "deathmatch", "0" );

    // Feedback for the latched cvar (consumed at the next restart).
    char buf[16];
    std::snprintf( buf, sizeof( buf ), "%d", mc );
    rt.cvars->cvar_full_set( "maxplayers", buf,
                             ::xash::cmd_cvar::FCVAR_LATCH );
    // XASH3DPP-STUB(chunk6): legacy ClearBits(sv_maxclients.flags, FCVAR_CHANGED)
    // after the latch write (sv_init.c:833) — cvar_full_set leaves FCVAR_CHANGED
    // set; needs a cmd_cvar clear-changed accessor.  Low impact (nothing polls
    // that bit at the milestone); tracked with the S10 inventory.

    const std::size_t floor = static_cast<std::size_t>( mc ) + 1;
    rt.arena.set_reserved( floor );      // alloc floor tracks maxclients + 1
    rt.arena.set_num_entities( floor );  // svgame.numEntities = maxclients + 1
    rt.bridge.max_clients = mc;
    rt.globals.maxClients = mc;
    rt.clients.maxclients = mc; // S9 — svs.clients active range / SV_Multicast

    // SV_UPDATE_BACKUP + svs.packet_entities realloc + per-client frames rings
    // (sv_init.c:821-827).  Sized from maxclients; freed in snapshot_shutdown.
    // XASH3DPP-STUB(chunk6-S9): svs.clients realloc + NET_Config land with the
    // full client array / netchan send path.
    ( void )snapshot_alloc_ring( rt );
}

bool spawn_server( ServerRuntime &rt, const char *mapname,
                   const char *startspot, bool background ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( mapname == nullptr || mapname[0] == '\0' )
    {
        host_error( rt, "SV_SpawnServer: empty map name" );
        return false;
    }
    if ( rt.maps == nullptr )
    {
        host_error( rt, "SV_SpawnServer: no map loader available" );
        return false;
    }

    setup_clients( rt );

    // SV_InitGame → SV_LoadProgs (idempotent; the DLL persists across maps).
    if ( !rt.game_loaded && !load_progs( rt, rt.cfg.game_dll ))
        return false;

    // Delta_Init (sv_init.c:946) already ran inside load_progs; the delta
    // tables never change between spawns, so the legacy per-spawn re-init is
    // a behavioural no-op (Q-18: the encoding is what matters).

    rt.persistent.initialized = true; // svs.initialized — set EARLY (quirk)
    rt.persistent.spawncount++;

    // XASH3DPP-STUB(chunk6-S9): Log_Open / Log_PrintServerVars (server log);
    // svs.challenge_salt regeneration (client challenges); svs.timestart
    // (needs the engine clock — Con_DPrintf level-load timing only).

    // Config-exec queue (mapchangecfgfile + maps/<name>_load.cfg).
    if ( rt.cvars != nullptr )
    {
        const char *cycle =
            rt.cvars->cvar_variable_string( "mapchangecfgfile" );
        char cmd[160];
        if ( cycle != nullptr && cycle[0] != '\0' )
        {
            std::snprintf( cmd, sizeof( cmd ), "exec %s\n", cycle );
            rt.cvars->cbuf_add_text( cmd );
        }
        std::snprintf( cmd, sizeof( cmd ), "exec maps/%s_load.cfg\n", mapname );
        rt.cvars->cbuf_add_text( cmd );

        // let's not have any servers with no name (sv_init.c:970-971): default an
        // empty hostname to the game description (legacy falls back to FS_Title;
        // the game_dir is the closest engine-side equivalent here).
        const char *hn = rt.cvars->cvar_variable_string( "hostname" );
        if ( hn == nullptr || hn[0] == '\0' )
        {
            const char *desc = rt.game.funcs().pfnGetGameDescription != nullptr
                                   ? rt.game.funcs().pfnGetGameDescription()
                                   : nullptr;
            rt.cvars->cvar_set( "hostname", ( desc != nullptr && desc[0] != '\0' )
                                                ? desc
                                                : rt.cfg.game_dir );
        }
    }

    // memset( &sv, 0 ) — wipe the per-level structure, then re-stamp.
    rt.level = LevelState{};
    rt.precache.clear();
    rt.level.time       = 1.0;  // sv.time = globals->time = 1.0 (spawn epoch)
    rt.globals.time     = 1.0f;
    rt.level.background  = background;

    // MSG_Init( &sv.signon ) (sv_init.c:987): rewind the signon buffer for the
    // new level (create_baselines refills it at activate).
    // XASH3DPP-STUB(chunk6-S9): sv.datagram / sv.multicast / sv.spec_datagram
    // sizebuf inits + svs.static_entities memset (send sub-slice).
    rt.signon.reset();
    snapshot_reset( rt ); // svs.baselines cleared per level (sv_init.c:995)

    // Gamemode consistency + skill clamp (console cvars; globals stamped).
    if ( rt.cvars != nullptr )
    {
        if ( rt.cvars->cvar_variable_value( "coop" ) != 0.0f )
            rt.cvars->cvar_set( "deathmatch", "0" );

        const int skill = clamp_int(
            0,
            static_cast<int>( rt.cvars->cvar_variable_value( "skill" ) + 0.5f ),
            3 );
        char sk[8];
        std::snprintf( sk, sizeof( sk ), "%d", skill );
        rt.cvars->cvar_set( "skill", sk );

        if ( rt.persistent.maxclients == 1 )
            rt.cvars->cvar_set( "sv_clienttrace", "1" );

        rt.globals.deathmatch = rt.cvars->cvar_variable_value( "deathmatch" );
        rt.globals.coop       = rt.cvars->cvar_variable_value( "coop" );
    }
    rt.globals.maxClients = rt.persistent.maxclients;

    // XASH3DPP-STUB(chunk6-S9): sv_background / cl_background DirectFullSet
    // (the background-map state the client UI reads).

    // sv.name = stripped map name.
    std::snprintf( rt.level.name, sizeof( rt.level.name ), "%s", mapname );
    strip_extension( rt.level.name );

    // Precache + static commands are allowed once the state is ss_loading.
    set_server_state( rt, ServerState::Loading );

    if ( startspot != nullptr )
        std::snprintf( rt.level.startspot, sizeof( rt.level.startspot ), "%s",
                       startspot );
    else
        rt.level.startspot[0] = '\0';

    // World model at precache slot WORLD_INDEX (1) + Mod_LoadWorld, which
    // calls back into MapLoader (world ownership stays in map_loader — Q-6).
    char world_path[80];
    std::snprintf( world_path, sizeof( world_path ), "maps/%s.bsp",
                   rt.level.name );
    const int world_slot = rt.precache.model_index( world_path );
    rt.precache.set_model_flags( static_cast<std::size_t>( world_slot ),
                                 ::xash::abi::k_res_fatalifmissing );

    ml::WorldLoadOptions opts;
    opts.is_world        = true;
    opts.multiplayer_crc = rt.persistent.maxclients > 1;
    opts.hull_bounds     = rt.hull_bounds;
    if ( !rt.maps->load_world( rt.level.name, opts ))
    {
        host_error( rt, "SV_SpawnServer: couldn't load world model" );
        return false;
    }
    const ml::WorldData *world = rt.maps->world();
    if ( world == nullptr )
    {
        host_error( rt, "SV_SpawnServer: null world after load" );
        return false;
    }
    rt.level.worldmap_crc = world->checksum();

    // XASH3DPP-STUB(chunk6): progs.dat CRC grab (ENGINE_QUAKE_COMPATIBLE only).

    // Submodel precache: "*1".."*(n-1)" at slots 2..n (sv_init.c:1046-1051).
    for ( std::size_t i = 1; i < world->submodels().size(); ++i )
    {
        char sub[16];
        std::snprintf( sub, sizeof( sub ), "*%zu", i );
        const int slot = rt.precache.model_index( sub );
        rt.precache.set_model_flags( static_cast<std::size_t>( slot ),
                                     ::xash::abi::k_res_fatalifmissing );
    }

    // Leave the low slots for clients only — SV_InitEdict each client edict.
    for ( int i = 0; i < rt.persistent.maxclients; ++i )
    {
        // XASH3DPP-STUB(chunk6-S9): svs.clients[i].state downgrade + edict
        // binding; the edict-slot init is the part spawn needs now.
        ::xash::abi::edict_t *ent =
            rt.arena.edict_num( static_cast<std::size_t>( i ) + 1 );
        if ( ent != nullptr )
            rt.arena.init_edict( ent );
    }

    // XASH3DPP-STUB(chunk6-S9): NET_MasterClear.

    // SV_UpdateMovevars( true ) (sv_init.c:1058): stamp the physics movevars
    // from the sv_* cvars before the settle frames run.
    sv_update_movevars( rt, true );

    install_world_bridge( rt, *world );

    // XASH3DPP-STUB(chunk6-S9): SV_GenerateTestPacket.
    return true;
}

void activate_server( ServerRuntime &rt, bool run_physics ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( !rt.persistent.initialized )
        return;

    // Cvar_SetValue( "sv_newunit", 0 ) (sv_init.c:582): the changelevel
    // new-unit latch is consumed once the level is activated.
    if ( rt.cvars != nullptr )
        rt.cvars->cvar_set( "sv_newunit", "0" );

    free_old_entities( rt );

    // Activate the DLL server code.
    rt.globals.time = static_cast<float>( rt.level.time );
    if ( rt.game.funcs().pfnServerActivate != nullptr )
        rt.game.funcs().pfnServerActivate(
            rt.arena.base(), static_cast<int>( rt.arena.num_entities() ),
            rt.persistent.maxclients );

    // The string pool switches to the dynamic arena only AFTER activate — the
    // strings the game allocates during ServerActivate are permanent
    // (SV_SetStringArrayMode true, sv_init.c:601).
    rt.strings.set_dynamic( true );

    // XASH3DPP-STUB(chunk6-S9): SV_CreateGenericResources (user resources).

    int num_frames;
    if ( run_physics )
    {
        num_frames         = rt.persistent.maxclients <= 1 ? 2 : 8;
        rt.level.frametime = k_sv_spawn_time;
    }
    else
    {
        rt.level.frametime = 0.001f;
        num_frames         = 1;
    }

    // Run some frames to let everything settle (SV_Physics per frame,
    // sv_init.c:617-619).  Legacy does NOT advance sv.time here — every settle
    // frame runs at the spawn epoch (sv.time == 1.0); only the per-frame
    // SV_RunGameFrame advances the clock later.
    for ( int i = 0; i < num_frames; ++i )
        sv_physics( rt );

    // SV_CreateBaseline fill (sv_init.c:622): populate baselines for delta.
    create_baselines( rt );
    // XASH3DPP-STUB(chunk6-S9): SV_CreateResourceList /
    // SV_TransferConsistencyInfo / per-client Netchan_Clear (send sub-slice).

    rt.globals.changelevel = 0; // svgame.globals->changelevel = false

    // sv.hostflags = 0 + oldmovevars snapshot (sv_init.c:663-668).  The
    // host.movevars_changed / HPAK_FlushHostQueue / Mod_FreeUnused steps stay
    // S9/host seams.
    rt.level.hostflags = 0;
    // memset( &svgame.oldmovevars, 0 ) (sv_init.c:644): zero the delta baseline
    // so the next SV_UpdateMovevars emits a FULL movevars delta to clients after
    // activate (legacy pairs this with host.movevars_changed = true).  A memcpy
    // here would make oldmovevars == movevars and suppress that initial delta.
    std::memset( &rt.oldmovevars, 0, sizeof( rt.oldmovevars ) );

    set_server_state( rt, ServerState::Active );
}

} // namespace xash::server
