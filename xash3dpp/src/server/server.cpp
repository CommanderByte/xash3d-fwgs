// xash3dpp — server subsystem implementation (Chunk 6)
// Legacy reference: engine/server/sv_main.c (SV_Init/SV_Shutdown shell) +
// engine/common/host_state.c (COM_LoadLevel/COM_LoadGame/COM_ChangeLevel
// dispatch, which the MapLoader FSM now routes through ILevelChangeExecutor).
//
// The Server owns the ServerRuntime aggregate (Q-2: the legacy sv/svs/svgame
// file-scope triple) and drives the lifecycle free functions over it.  It is
// the MapLoader's level-change executor: exec_load_level is the full
// SV_SpawnServer → spawn_entities → SV_ActivateServer chain.
//
// Existing subsystems used:
//   xash3dpp_map_loader — world ownership (Q-6) + the FSM that drives us
//   xash3dpp_core       — logging, thread-role asserts

#include <xash3dpp/server/server.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/physics.hpp>

#include <cstddef>

namespace xash::server {

namespace {

// Copy a (non-terminated) string_view into a MAX_QPATH-ish fixed buffer.
void copy_name( char *dst, std::size_t cap, std::string_view src ) noexcept
{
    const std::size_t n = src.size() < cap - 1 ? src.size() : cap - 1;
    for ( std::size_t i = 0; i < n; ++i )
        dst[i] = src[i];
    dst[n] = '\0';
}

} // namespace

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct Server::Impl
{
    ServerRuntime rt;
    ServerStats   stats_;
};

Server::Server() : impl_{ std::make_unique<Impl>() } {}
Server::~Server() = default;

Server::Server( Server && ) noexcept            = default;
Server &Server::operator=( Server && ) noexcept = default;

bool Server::init( const ServerInitParams &params )
{
    // OQ-9 threading posture: every server entry point is main-thread.
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );

    ServerRuntime &rt = impl_->rt;

    rt.cfg.game_dir     = params.game_dir;
    rt.cfg.game_dll     = params.game_dll;
    rt.cfg.dedicated    = params.dedicated;
    rt.cfg.developer    = params.developer;
    rt.cfg.peoei_broken = params.peoei_broken;
    if ( params.max_edicts != 0 )
        rt.cfg.max_edicts = params.max_edicts;
    rt.cfg.host_error     = params.host_error;
    rt.cfg.host_error_ctx = params.host_error_ctx;

    rt.cvars = params.cvars;
    rt.fs    = params.fs;
    rt.maps  = params.maps;
    rt.net   = params.net;

    // The game DLL loads lazily at the first SV_SpawnServer (legacy
    // SV_InitGame) — init only wires the dependencies.
    return true;
}

void Server::shutdown()
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );

    // SV_Shutdown → SV_UnloadProgs runs the full unwind: deactivate the live
    // server (if any), then release the game binding.  Idempotent — a never-
    // loaded server early-returns.
    unload_progs( impl_->rt );

    // XASH3DPP-STUB(chunk6-S9): SV_Shutdown's final message ×2 / master
    // heartbeat shutdown / client-ring + testpacket free / log close land with
    // the client machinery.
}

bool Server::active() const noexcept
{
    return impl_->rt.level.state == ServerState::Active;
}

bool Server::initialized() const noexcept
{
    return impl_->rt.persistent.initialized;
}

const ServerStats &Server::stats() const noexcept
{
    return impl_->stats_;
}

bool Server::exec_load_level( std::string_view map, bool background ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );

    ServerRuntime &rt = impl_->rt;

    char name[64]; // MAX_QPATH-class stripped map name
    copy_name( name, sizeof( name ), map );

    // SV_SetStringArrayMode( false ) (sv_init.c:1105): force the string pool
    // back to the STATIC half before spawn so this level's entity strings land
    // there.  deactivate_server empties the pool but leaves it in dynamic mode,
    // so without this the 2nd+ map's entity string_t's would be written into —
    // and later clobbered in — the dynamic (runtime-churn) half.
    rt.strings.set_dynamic( false );

    if ( !spawn_server( rt, name, nullptr, background ) )
        return false;

    const ::xash::map_loader::WorldData *world = rt.maps->world();
    if ( world == nullptr )
        return false;

    spawn_entities( rt, *world );
    activate_server( rt, /*run_physics=*/true );

    impl_->stats_.frames_run.fetch_add( 1, std::memory_order_relaxed );
    return active();
}

void Server::frame( double host_frametime ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    host_server_frame( impl_->rt, host_frametime );
}

bool Server::exec_load_game( std::string_view /*map*/ ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    // XASH3DPP-STUB(chunk8): savegame restore (SV_LoadGame staging + the
    // spawn/activate(false) settle-frame path) lands with the save/restore
    // chunk behind this executor seam.
    return false;
}

bool Server::exec_change_level( std::string_view /*map*/,
                                std::string_view /*landmark*/,
                                bool /*background*/ ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    // XASH3DPP-STUB(chunk8): landmark transition (adjacent-level save staging,
    // CHANGE_LEVEL fixups) lands with the save/restore chunk.
    return false;
}

} // namespace xash::server
