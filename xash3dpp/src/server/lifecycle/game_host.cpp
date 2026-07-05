// xash3dpp — game-DLL host orchestration: SV_LoadProgs / SV_UnloadProgs /
// SV_DeactivateServer / Host_SetServerState (Chunk 6 S7)
// Legacy reference: engine/server/sv_game.c :5171-5366, sv_init.c :35-39,
// :682-722.
//
// Existing subsystems used:
//   xash3dpp_memory     — svgame.mempool equivalent ("server_game" pool)
//   xash3dpp_cmd_cvar   — host_gameloaded/host_serverstate mirrors, the
//                         FCVAR_EXTDLL unlink dance, cfg-exec queueing
//   xash3dpp_networking — DeltaTables (Delta_Init/Delta_Shutdown)
//   xash3dpp_core       — logging, thread-role asserts

#include <xash3dpp/private/server/lifecycle.hpp>

#include <xash3dpp/cmd_cvar/command.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/pmove.hpp>
#include <xash3dpp/private/server/world_links.hpp>

#include <cstdio>

namespace xash::server {

namespace {

void host_error( ServerRuntime &rt, const char *msg )
{
    if ( rt.cfg.host_error != nullptr )
        rt.cfg.host_error( rt.cfg.host_error_ctx, msg );
    else
        ::xash::core::log_error( "server", msg );
}

// SV_FreePrivateData callback half (sv_game.c:961-976): the game must see
// pfnOnFreeEntPrivateData before the engine releases the block.
void private_data_releaser( void *ctx, ::xash::abi::edict_t *ed )
{
    auto *game = static_cast<GameDll *>( ctx );
    if ( game->loaded() && game->has_new_api() &&
         game->new_funcs().pfnOnFreeEntPrivateData != nullptr )
        game->new_funcs().pfnOnFreeEntPrivateData( ed );
}

// SV_InitOperatorCommands / SV_KillOperatorCommands (sv_cmds.c:1053/1093).
// XASH3DPP-STUB(chunk6): the operator console surface (kick/status/
// serverinfo/...) lands with the client machinery in S9; these seams keep
// the legacy registration/removal ORDER points so S9 only fills bodies.
void register_operator_commands( ServerRuntime & ) {}
void kill_operator_commands( ServerRuntime & ) {}

} // namespace

void set_server_state( ServerRuntime &rt, ServerState state ) noexcept
{
    // Host_SetServerState (sv_init.c:35-39): the cvar mirror carries the
    // raw enum integer.
    if ( rt.cvars != nullptr )
    {
        char buf[8];
        std::snprintf( buf, sizeof( buf ), "%i", static_cast<int>( state ));
        rt.cvars->cvar_full_set( "host_serverstate", buf,
                                 ::xash::cmd_cvar::FCVAR_READ_ONLY );
    }
    rt.level.state = state;

    // Mirror into the bridge so the SV_SetModel ss_active guard can read it
    // (the ABI shim never includes lifecycle.hpp).
    rt.bridge.server_state = static_cast<int>( state );

    // Precache index registration is load-time only during ss_loading
    // (sv_init.c:131-137 et al.).
    rt.precache.set_loading( state == ServerState::Loading );
}

bool load_progs( ServerRuntime &rt, const char *dll_path ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // The DLL persists across map changes (sv_game.c:5227-5228).
    if ( rt.game_loaded )
        return true;

    if ( rt.fs == nullptr )
    {
        ::xash::core::log_error( "server",
                                 "load_progs: no filesystem available" );
        return false;
    }

    rt.game_pool = ::xash::memory::create_pool( "server_game" );
    if ( !rt.game_pool )
        return false;

    // Bridge state behind the 159 slots — installed before the handshake
    // so callbacks made from inside GetEntityAPI*/GameInit already work
    // (legacy gEngfuncs is a static that always exists).
    rt.bridge.arena     = &rt.arena;
    rt.bridge.strings   = &rt.strings;
    rt.bridge.globals   = &rt.globals;
    rt.bridge.game      = &rt.game;
    rt.bridge.precache  = &rt.precache;
    rt.bridge.misc_pool = rt.game_pool;
    rt.bridge.max_clients    = rt.persistent.maxclients;
    rt.bridge.dedicated      = rt.cfg.dedicated;
    rt.bridge.developer      = rt.cfg.developer;
    rt.bridge.game_dir       = rt.cfg.game_dir;
    rt.bridge.host_error     = rt.cfg.host_error;
    rt.bridge.host_error_ctx = rt.cfg.host_error_ctx;
    rt.bridge.clients        = &rt.clients; // S9 — messaging pfn slots reach it
    rt.bridge.delta          = &rt.delta;   // S9 — reliable-event null-compression
    install_engine_bridge( &rt.bridge );

    // S9: bind the multicast scratch buffer + reset per-client staging.
    clients_init( rt.clients );

    rt.precache.set_error_hook( rt.cfg.host_error, rt.cfg.host_error_ctx );

    // Local table copy semantics + the peoei bugcomp patch
    // (sv_game.c:5248-5254).
    rt.engine_table = build_engine_table( rt.cfg.peoei_broken );
    rt.globals      = {};

    if ( !rt.game.load( dll_path, &rt.engine_table, &rt.globals ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                            "load_progs: can't initialize %s (error %d)",
                            dll_path,
                            static_cast<int>( rt.game.last_error( )));
        install_engine_bridge( nullptr );
        ::xash::memory::destroy_pool( rt.game_pool );
        rt.game_pool = {};
        return false;
    }

    rt.arena.set_private_releaser( private_data_releaser, &rt.game );

    register_operator_commands( rt );

    // XASH3DPP-STUB(chunk6): Mod_InitStudioAPI (sv_game.c:5326) — the
    // OQ-2 studio provider seam lands with Chunk 7 content.
    // XASH3DPP-STUB(chunk6): SV_InitPhysicsAPI (physint.h negotiation) —
    // S8 physics; version-reject is only a warning in legacy.
    // XASH3DPP-STUB(chunk6): SV_InitSaveRestore (SV_SaveGameComment grab)
    // — Chunk 8 save seam.

    // Legacy sets pStringBase to a static "" first (sv_game.c:5336);
    // SV_AllocStringPool replaces it below.
    rt.globals.pStringBase = "";
    rt.globals.maxEntities = static_cast<int>( rt.cfg.max_edicts );
    rt.globals.maxClients  = rt.persistent.maxclients;

    const std::size_t reserved =
        static_cast<std::size_t>( rt.persistent.maxclients ) + 1;
    bool ok = rt.arena.init( rt.game_pool, rt.cfg.max_edicts, reserved );

    // svs.baselines: Z_Calloc(entity_state_t * max_edicts) (sv_game.c:5342).
    // svs.static_entities (pfnMakeStatic) remains a later snapshot sub-slice.
    if ( ok )
        ok = snapshot_alloc_baselines( rt );
    if ( ok )
        ok = snapshot_alloc_signon( rt ); // sv.signon buffer (baselines/precache)
    rt.bridge.snapshot = &rt.snapshot; // pfnCreateInstancedBaseline reaches here

    if ( ok )
        ok = rt.precache.init( rt.game_pool );

    if ( rt.cvars != nullptr )
    {
        rt.cvars->cvar_full_set( "host_gameloaded", "1",
                                 ::xash::cmd_cvar::FCVAR_READ_ONLY );
        rt.cvars->set_server_dll_loaded( true );
    }

    if ( ok )
        ok = rt.strings.init( rt.game_pool, rt.cfg.max_edicts );
    if ( ok )
        rt.globals.pStringBase = rt.strings.base();
    else
    {
        // Legacy cannot reach this (pool allocation Host_Errors); unwind
        // deterministically per Q-5.
        host_error( rt, "load_progs: server pool allocation failed" );
        rt.game_loaded = true; // let unload_progs run the full unwind
        unload_progs( rt );
        return false;
    }

    // Guarded like the SV_SpawnServer call site (sv_init.c:971) — the
    // table slot may legitimately be null only in broken DLLs.
    if ( rt.game.funcs().pfnGetGameDescription != nullptr )
        ::xash::core::logf( ::xash::core::LogLevel::Info, "server",
                            "Dll loaded for game \"%s\"",
                            rt.game.funcs().pfnGetGameDescription( ));

    // All done, initialize game (sv_game.c:5355).
    rt.game.funcs().pfnGameInit();
    rt.game_initialized = true;

    // SV_InitClientMove (sv_game.c:5358): enumerate the hull bounds the trace
    // kernel needs and allocate the single player-move working set (legacy
    // Mod_Init allocates svgame.pmove once; freed in unload_progs).  The PM_*
    // callback table it exposes is wired by the P3 trace family.
    rt.hull_bounds = query_hull_bounds( rt.game.funcs());
    rt.pmove       = ::xash::memory::pool_ptr<::xash::abi::playermove_t>(
        ::xash::memory::pool_new<::xash::abi::playermove_t>( rt.game_pool ));
    if ( rt.pmove == nullptr )
    {
        host_error( rt, "load_progs: playermove_t allocation failed" );
        rt.game_loaded = true;
        unload_progs( rt );
        return false;
    }

    // Delta_Init (sv_game.c:5360) — legacy hard-errors from inside
    // Delta_Load when delta.lst is unreadable.
    if ( !rt.delta.init( *rt.fs ))
    {
        host_error( rt, "load_progs: Delta_Init failed (delta.lst)" );
        rt.game_loaded = true;
        unload_progs( rt );
        return false;
    }

    rt.game.funcs().pfnRegisterEncoders();

    rt.game_loaded = true;
    return true;
}

void unload_progs( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( !rt.game_loaded )
        return;

    deactivate_server( rt );

    rt.delta.clear(); // Delta_Shutdown

    // Cvar_PrepareToUnlink( FCVAR_EXTDLL ) BEFORE pfnGameShutdown — the
    // game may still touch its cvars during shutdown (sv_game.c:5184-5187).
    if ( rt.cvars != nullptr )
        rt.cvars->cvar_prepare_to_unlink( ::xash::cmd_cvar::FCVAR_EXTDLL );

    // Shutdown is delivered only when Init ran (pairing invariant; the
    // pre-GameInit unwind paths are unreachable in legacy).
    if ( rt.game_initialized && rt.game.has_new_api() &&
         rt.game.new_funcs().pfnGameShutdown != nullptr )
        rt.game.new_funcs().pfnGameShutdown();

    if ( rt.cvars != nullptr )
    {
        rt.cvars->cvar_full_set( "host_gameloaded", "0",
                                 ::xash::cmd_cvar::FCVAR_READ_ONLY );
        rt.cvars->set_server_dll_loaded( false );
    }

    // XASH3DPP-STUB(chunk6): free svs.static_entities / svs.baselines —
    // S9, together with their allocation.

    kill_operator_commands( rt );

    if ( rt.cvars != nullptr )
    {
        rt.cvars->unlink_pending_cvars();
        rt.cvars->cmd_unlink( ::xash::cmd_cvar::FCMD_EXTDLL );
    }

    // Engine-owned cvar replacement strings + the pfnCVarRegister chain
    // (see engine_bridge.hpp) — must precede game_pool destruction.
    reset_external_cvars( rt.bridge );

    rt.strings.shutdown(); // SV_FreeStringPool

    // OQ-2: Mod_ResetStudioAPI has no xash3dpp counterpart until the
    // Chunk 7 studio provider exists.

    // Arena teardown runs BEFORE the library goes away and with the
    // releaser still installed: any edict that kept private data past
    // deactivation must see pfnOnFreeEntPrivateData while the DLL is
    // loaded (legacy delivers these inside SV_DeactivateServer →
    // SV_FreeEdicts; once the S7b deactivate body lands, this sweep is a
    // no-op for the common path).
    rt.arena.shutdown();
    rt.arena.set_private_releaser( nullptr, nullptr );
    rt.precache.shutdown();
    snapshot_shutdown( rt ); // Z_Free svs.baselines + packet_entities + frames
    rt.pmove.reset();        // free svgame.pmove

    rt.game.unload(); // COM_FreeLibrary

    install_engine_bridge( nullptr );
    ::xash::memory::destroy_pool( rt.game_pool ); // Mem_FreePool
    rt.game_pool = {};

    // memset( &svgame, 0, sizeof( svgame )) equivalent.
    rt.globals          = {};
    rt.bridge           = {};
    rt.hull_bounds      = {};
    rt.game_loaded      = false;
    rt.game_initialized = false;
}

void deactivate_server( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // Legacy quirk (sv_init.c:685-694): both cfg execs are queued BEFORE
    // the initialized/dead guard — a dead server still queues them.
    if ( rt.cvars != nullptr )
    {
        const char *cycle = rt.cvars->cvar_variable_string( "disconcfgfile" );
        char        cmd[128];
        if ( cycle != nullptr && cycle[0] != '\0' )
        {
            std::snprintf( cmd, sizeof( cmd ), "exec %s\n", cycle );
            rt.cvars->cbuf_add_text( cmd );
        }
        if ( rt.level.name[0] != '\0' )
        {
            std::snprintf( cmd, sizeof( cmd ), "exec maps/%s_unload.cfg\n",
                           rt.level.name );
            rt.cvars->cbuf_add_text( cmd );
        }
    }

    if ( !rt.persistent.initialized || rt.level.state == ServerState::Dead )
        return;

    // SV_InactivateClients — XASH3DPP-STUB(chunk6-S9): drops connected
    // clients back to a reconnect state; no client array until S9.

    rt.globals.time = static_cast<float>( rt.level.time );
    if ( rt.game.funcs().pfnServerDeactivate != nullptr )
        rt.game.funcs().pfnServerDeactivate();

    set_server_state( rt, ServerState::Dead );

    // SV_FreeEdicts: release every live edict — free_edict fires the private-
    // data releaser (pfnOnFreeEntPrivateData) while the DLL is still loaded.
    for ( std::size_t i = 0; i < rt.arena.num_entities(); ++i )
    {
        ::xash::abi::edict_t *ed = rt.arena.edict_num( i );
        if ( ed == nullptr || EntityView( ed ).freed() )
            continue;
        WorldLinks::unlink_edict( ed );
        rt.arena.free_edict( ed, rt.level.time );
    }

    pm_clear_phys_ents( rt ); // PM_ClearPhysEnts( svgame.pmove )

    // SV_EmptyStringPool( true ) + Mem_EmptyPool( svgame.stringspool ): the
    // per-level dynamic arena is reset; the static arena survives.
    rt.strings.empty_pool( true );

    // per-client frame release — XASH3DPP-STUB(chunk6-S9).

    rt.globals.maxEntities = static_cast<int>( rt.cfg.max_edicts );
    rt.globals.maxClients  = rt.persistent.maxclients;
    rt.arena.set_num_entities(
        static_cast<std::size_t>( rt.persistent.maxclients ) + 1 );

    // Null the world globals — the stale-world guard so a game DLL that peeks
    // at globals->mapname after deactivate sees "no world" (sv_init.c:720-721).
    rt.globals.startspot = 0;
    rt.globals.mapname   = 0;

    // Unbind the world-interaction env so nothing refines against a world
    // MapLoader may free before the next spawn.
    rt.models.bind( nullptr, nullptr );
    rt.move_env = MoveEnv{};
    rt.link_env = LinkEnv{};
    rt.hooks.bind( &rt.game, nullptr );
    rt.bridge.move_env = nullptr;
    rt.bridge.links    = nullptr;
    rt.bridge.link_env = nullptr;
}

} // namespace xash::server
