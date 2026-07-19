// xash3dpp — Host implementation
// Legacy reference: engine/common/host.c
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations (host pool)
//   xash3dpp_filesystem — VFS init / game directory activation
//   xash3dpp_core       — core::log, core::ErrorCode, core::Clock
//   xash3dpp_cmd_cvar   — command buffer, cvar registry
//   xash3dpp_map_loader — map-load FSM
//   xash3dpp_platform   — platform::get_time(), platform::console::read_line()

#include <xash3dpp/host/host.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/clock.hpp>
#include <xash3dpp/core/error.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/server/server.hpp>
#include <xash3dpp/platform/crash.hpp>
#include <xash3dpp/platform/platform.hpp>

#include <array>
#include <cstring>

namespace xash {

using namespace xash::memory;
using namespace xash::filesystem;

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct Host::Impl
{
    // Configuration — owned copies of the non-owning HostInitParams fields.
    std::string   rootdir, basedir, gamedir, rodir;
    bool          dedicated = false;
    int           developer = 0;
    std::uint32_t bugcomp   = 0;

    HostStatus  status     = HostStatus::Init;
    HostStats   stats_     {};              // always-on snapshot; status field mirrored here

    // Frame-abort propagation (Quirk Q-3, OQ-1 hybrid).
    bool                  frame_abort_pending = false;
    ::xash::core::ErrorCode       frame_abort_code    = ::xash::core::ErrorCode::Ok;
    std::array<char, ::xash::limits::host_frame_abort_detail_buf> frame_abort_detail {};

    // Injected deps — non-owning; null = standalone / test mode.
    // Must outlive this Impl.  Only written in init(); never written again.
    cmd_cvar::CmdCvarContext  *cmd_cvar   = nullptr;
    ::xash::core::Clock               *clock      = nullptr;
    ::xash::server::Server            *server     = nullptr; // @lifetime: engine (EngineContext owns; borrowed for frame())
    MapLoader                 *map_loader = nullptr;
    filesystem::Filesystem    *ext_fs     = nullptr;

    // Subsystem owned by the host in standalone mode only.
    // NOTE: pool is the first subsystem created and the last destroyed.
    // Impl itself is system-allocated (make_unique) because the pool does
    // not exist yet when Impl is constructed.
    PoolHandle pool;
    filesystem::Filesystem own_fs;  // used when ext_fs == nullptr

    // Convenience: returns the active filesystem (owned or injected).
    filesystem::Filesystem &fs() noexcept
    {
        return ext_fs ? *ext_fs : own_fs;
    }

    void shutdown() noexcept
    {
        ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
        if ( !pool ) return;  // already shut down or never initialised

        // Server shutdown is EngineContext's job (it owns the Server and its
        // dependency-ordered teardown — engine_context.cpp); drop the borrow.
        server = nullptr;
        // TODO Chunk 12: Client::shutdown()
        // Networking is owned by EngineContext, not Host — see engine_context.cpp.

        if ( map_loader ) { map_loader->shutdown(); map_loader = nullptr; }
        if ( clock )      { clock->shutdown();      clock      = nullptr; }
        if ( cmd_cvar )   { cmd_cvar->shutdown();   cmd_cvar   = nullptr; }

        // Filesystem: only shut down if we own it.
        if ( !ext_fs ) own_fs.shutdown();
        ext_fs = nullptr;

        destroy_pool( pool );
        pool = k_null_pool;

        status        = HostStatus::Shutdown;
        stats_.status = status;
    }
};

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

Host::Host()  : impl_{ std::make_unique<Impl>() } {}
Host::~Host() = default;

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

bool Host::init(const HostInitParams& p)
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl& s = *impl_;

    // Copy config from non-owning string_views into owned strings.
    s.rootdir   = std::string( p.rootdir );
    s.basedir   = std::string( p.basedir );
    s.gamedir   = std::string( p.gamedir );
    s.rodir     = std::string( p.rodir );
    s.dedicated = p.dedicated;
    s.developer = p.developer;
    s.bugcomp   = p.bugcomp;

    // Store injected deps (non-owning; null = standalone / test mode).
    s.cmd_cvar   = p.cmd_cvar;
    s.clock      = p.clock;
    s.map_loader = p.map_loader;
    s.server     = p.server;
    s.ext_fs     = p.filesystem;

    s.status        = HostStatus::Init;
    s.stats_.status = s.status;

    // --- Memory ----------------------------------------------------------
    s.pool = create_pool( "host" );
    if ( !s.pool )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "host",
                   "create_pool(\"host\") failed (registry full)" );
        return false;
    }

    // --- Filesystem (standalone mode only) -------------------------------
    // In EngineContext mode, FS is initialised before Host by EngineContext::init().
    if ( !s.ext_fs )
    {
        const std::string_view basedir = s.basedir.empty() ? s.gamedir : s.basedir;
        const std::string_view gamedir = s.gamedir.empty() ? s.basedir : s.gamedir;

        if ( !s.own_fs.init( s.rootdir, basedir, gamedir, s.rodir ) )
        {
            ::xash::core::log( ::xash::core::LogLevel::Error, "host", "Filesystem::init failed" );
            destroy_pool( s.pool );
            s.pool = k_null_pool;
            return false;
        }

        const filesystem::SearchPathFlags mount_flags =
            filesystem::SearchPathFlags::MountHD | filesystem::SearchPathFlags::MountLV;

        if ( !gamedir.empty() && !s.own_fs.activate_game( gamedir, mount_flags ) )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Warning, "host",
                        "game directory '%.*s' not found, running in base mode",
                        static_cast<int>( gamedir.size() ), gamedir.data() );
        }
    }

    // --- Cmd / Cvar commands & cvars -------------------------------------
    // When cmd_cvar is null (standalone / test mode) registration is skipped.
    // TODO Chunk 3: register host lifecycle cvars (host_developer, host_gameloaded,
    //   host_clientloaded, host_limitlocal, con_gamemaps, host_allow_materials, ...)
    //   and commands (quit, exit, memlist, host_error, sys_error, crash).

    // Networking is owned and initialised by EngineContext — see engine_context.cpp.

    // --- Server ----------------------------------------------------------
    // Server::init runs in EngineContext::init (dependency order: after
    // host); Host only borrows the pointer and drives frame().

    // --- Client (Chunk 12, non-dedicated only) ----------------------------
    // TODO Chunk 12: if (!s.dedicated) Client::init()

    if ( s.developer > 0 )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Info, "host",
                    "host init complete  rootdir='%s'  game='%s'  "
                    "dedicated=%d  developer=%d",
                    s.rootdir.c_str(), s.gamedir.c_str(),
                    static_cast<int>( s.dedicated ), s.developer );
    }

    s.status        = HostStatus::Running;
    s.stats_.status = s.status;
    return true;
}

// ---------------------------------------------------------------------------
// RunFrame
// ---------------------------------------------------------------------------

void Host::RunFrame()
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    Impl& s = *impl_;
    if ( s.status == HostStatus::Shutdown ) return;

    // Frame-abort recovery (Quirk Q-3, OQ-1) — runs at frame top so all
    // destructors from the aborted frame have already executed.
    if ( s.frame_abort_pending )
    {
        ::xash::core::log( ::xash::core::LogLevel::Warning, "host",
                   "frame abort recovered; subsystem cleanup pending" );
        // TODO Chunk 6/12: SV_Shutdown(), CL_Drop(), CL_ClearEdicts(), Mod_FreeAll().
        s.frame_abort_pending   = false;
        s.frame_abort_code      = ::xash::core::ErrorCode::Ok;
        s.frame_abort_detail[0] = '\0';
    }

    // --- Clock tick (frame gate) ----------------------------------------
    // Advances realtime/frametime and applies the FPS/throttle/sleep policy
    // (legacy Host_FilterTime + Host_CalcSleep, owned by core::Clock).
    // false → not yet time for a frame; skip this tick entirely.
    if ( s.clock && !s.clock->tick() ) return;

    // --- Platform event pump -------------------------------------------
    // TODO Chunk 12: Platform::PollEvents() (client-side input / window events)

    // --- Command buffer -------------------------------------------------
    // cbuf_execute MUST run before map_loader::run_frame_step so that commands
    // issued this frame (e.g. "map") take effect in the same frame's FSM step.
    if ( s.cmd_cvar ) s.cmd_cvar->cbuf_execute();

    // --- Map-load FSM step ----------------------------------------------
    if ( s.map_loader ) s.map_loader->run_frame_step();

    // --- Dedicated stdin ------------------------------------------------
    // OQ-9: read a line from stdin and push it into the command buffer.
    // TODO: platform::console::read_line() + cbuf_add_text + cbuf_execute

    // --- Server frame ---------------------------------------------------
    // Host_ServerFrame equivalent (sv_main.c:678 via Server::frame): no-ops
    // internally until the server is initialized, so a menu-only or
    // standalone-test host ticks through harmlessly.
    if ( s.server ) s.server->frame( s.clock ? s.clock->frametime() : 0.0 );

    // --- Client frame (non-dedicated) -----------------------------------
    // TODO Chunk 12: Client::RunFrame()
}

// ---------------------------------------------------------------------------
// RequestShutdown
// ---------------------------------------------------------------------------

void Host::RequestShutdown(const char* /*reason*/) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    impl_->status        = HostStatus::Shutdown;
    impl_->stats_.status = impl_->status;
}

void Host::shutdown() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    impl_->shutdown();
}

// ---------------------------------------------------------------------------
// signal_frame_abort — Quirk Q-3, Resolved-decision OQ-1
// ---------------------------------------------------------------------------

void Host::signal_frame_abort(::xash::core::ErrorCode code,
                              std::string_view detail) noexcept
{
    // Main-only by contract (host.hpp): the Host_Error ABI shim reaches this
    // on the game-DLL call stack, which is always Main (HB-3 assert added
    // 2026-07-19; the threading doc's recorded enforcement gap).
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    Impl& s = *impl_;

    // Quirk Q-4: recursive abort within the same frame escalates to process abort.
    // Legacy: `if (host.errorframe == host.framecount) Sys_Error(...)`
    // Decision ref: host-boundary.md Resolved-decision OQ-1
    if ( s.frame_abort_pending )
    {
        ::xash::platform::crash::print_trace();
        XASH_FATAL( false, "recursive frame abort — escalating to process abort" );
    }

    s.frame_abort_pending = true;
    s.frame_abort_code    = code;
    const std::size_t n   = detail.size() < s.frame_abort_detail.size() - 1
                            ? detail.size()
                            : s.frame_abort_detail.size() - 1;
    std::memcpy( s.frame_abort_detail.data(), detail.data(), n );
    s.frame_abort_detail[n] = '\0';
}

// ---------------------------------------------------------------------------
// Main — full lifecycle in one call
// ---------------------------------------------------------------------------

int Host::Main(const HostArgs& args)
{
    // Build a HostInitParams from the launcher-provided HostArgs.
    // All dependency pointers are null: standalone / no EngineContext.
    HostInitParams p;
    p.rootdir   = args.rootdir;
    p.basedir   = args.basedir;
    p.gamedir   = args.gamedir;
    p.rodir     = args.rodir;
    p.dedicated = args.dedicated;
    p.developer = args.developer;
    p.bugcomp   = args.bugcomp;
    // dep pointers intentionally left null (standalone path)

    if ( !init( p ) ) return 1;

    while ( impl_->status != HostStatus::Shutdown )
        RunFrame();

    impl_->shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

HostStatus      Host::status()              const noexcept { return impl_->status; }
bool            Host::dedicated()           const noexcept { return impl_->dedicated; }
double          Host::realtime()            const noexcept
{
    // Forward through Clock when available; fall back to platform time.
    return impl_->clock ? impl_->clock->realtime() : ::xash::platform::get_time();
}
bool            Host::frame_abort_pending() const noexcept { return impl_->frame_abort_pending; }
::xash::core::ErrorCode Host::frame_abort_code()    const noexcept { return impl_->frame_abort_code; }
const HostStats& Host::stats()              const noexcept { return impl_->stats_; }

} // namespace xash
