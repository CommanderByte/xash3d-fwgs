// xash3dpp — EngineContext init/shutdown + global accessor bookend.
//
// Decision ref: docs/design/decisions-architecture.md §ENGINE_CONTEXT (Q-2),
//               docs/boundaries/host-boundary.md  Resolved-decision OQ-10
//
// Construction contract:
//   set_current_engine_context(this) is called ONLY after every subsystem
//   has initialised successfully, so the global accessor is never observable
//   in a partially-initialised state.
//
// Teardown contract:
//   set_current_engine_context(nullptr) is called FIRST in shutdown(), before
//   any subsystem teardown, so C-ABI callers cannot reach a half-torn-down
//   context.

#include <xash3dpp/host/engine_context.hpp>
#include <xash3dpp/abi/engine_context_accessor.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/clock.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/host/host.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>

namespace xash {

bool EngineContext::init(const EngineContextInitParams &p) noexcept
{
    // --- Filesystem -------------------------------------------------------
    if ( !filesystem.init( p.rootdir, p.basedir, p.gamedir, p.rodir ) )
    {
        core::log( core::LogLevel::Error, "engine_context", "Filesystem::init failed" );
        return false;
    }

    // --- Cmd / Cvar -------------------------------------------------------
    {
        cmd_cvar::CmdCvarInitParams cp;
        cp.trust_oracle  = p.trust_oracle;
        cp.compat_policy = p.compat_policy;
        if ( !cmd_cvar.init( cp ) )
        {
            core::log( core::LogLevel::Error, "engine_context", "CmdCvarContext::init failed" );
            filesystem.shutdown();
            return false;
        }
    }

    // --- Clock ------------------------------------------------------------
    {
        core::ClockInitParams cp;
        cp.cmd_cvar  = &cmd_cvar;
        cp.dedicated = p.dedicated;
        if ( !clock.init( cp ) )
        {
            core::log( core::LogLevel::Error, "engine_context", "Clock::init failed" );
            cmd_cvar.shutdown();
            filesystem.shutdown();
            return false;
        }
    }

    // --- Networking ---------------------------------------------------------
    {
        // Winsock lifetime is bracketed by EngineContext, not NetworkContext:
        // socket_init() is ref-counted and must precede any socket creation
        // (NetworkContext::init only creates its pool; config() opens sockets).
        platform::socket_init();
        networking::NetworkInitParams np;
        np.sockets   = p.sockets ? p.sockets : &platform::default_platform_sockets();
        np.dedicated = p.dedicated;
        if ( !networking.init( np ) )
        {
            core::log( core::LogLevel::Error, "engine_context", "NetworkContext::init failed" );
            platform::socket_shutdown();
            clock.shutdown();
            cmd_cvar.shutdown();
            filesystem.shutdown();
            return false;
        }
    }

    // --- MapLoader --------------------------------------------------------
    if ( !map_loader.init( MapLoaderInitParams{ .filesystem = &filesystem } ) )
    {
        core::log( core::LogLevel::Error, "engine_context", "MapLoader::init failed" );
        networking.shutdown();
        platform::socket_shutdown();
        clock.shutdown();
        cmd_cvar.shutdown();
        filesystem.shutdown();
        return false;
    }

    // --- Host -------------------------------------------------------------
    {
        HostInitParams hp;
        hp.rootdir    = p.rootdir;
        hp.basedir    = p.basedir;
        hp.gamedir    = p.gamedir;
        hp.rodir      = p.rodir;
        hp.dedicated  = p.dedicated;
        hp.developer  = p.developer;
        hp.bugcomp    = p.bugcomp;
        hp.cmd_cvar   = &cmd_cvar;
        hp.clock      = &clock;
        hp.map_loader = &map_loader;
        hp.filesystem = &filesystem;
        if ( !host.init( hp ) )
        {
            core::log( core::LogLevel::Error, "engine_context", "Host::init failed" );
            map_loader.shutdown();
            networking.shutdown();
            platform::socket_shutdown();
            clock.shutdown();
            cmd_cvar.shutdown();
            filesystem.shutdown();
            return false;
        }
    }

    // --- Server -----------------------------------------------------------
    {
        server::ServerInitParams sp;
        sp.cvars        = &cmd_cvar;
        sp.fs           = &filesystem;
        sp.maps         = &map_loader;
        sp.net          = &networking;
        sp.dedicated    = p.dedicated;
        sp.developer    = p.developer;
        // game_dll / game_dir + the Q-5 host_error hook are resolved by the
        // host at spawn time (COM_GetCommonLibraryPath, LIBRARY_SERVER — the
        // real DLL smoke test is S15); an empty path keeps init inert.
        if ( !server.init( sp ) )
        {
            core::log( core::LogLevel::Error, "engine_context", "Server::init failed" );
            host.shutdown();
            map_loader.shutdown();
            networking.shutdown();
            platform::socket_shutdown();
            clock.shutdown();
            cmd_cvar.shutdown();
            filesystem.shutdown();
            return false;
        }
        // The server becomes the MapLoader's level-change executor: `map` /
        // `changelevel` / `load` now bring up a real server (COM_LoadLevel).
        map_loader.set_level_executor( &server );
    }

    // All subsystems up — expose the accessor.
    bugcomp = p.bugcomp;
    xash::abi::set_current_engine_context( this );
    return true;
}

void EngineContext::shutdown() noexcept
{
    // Nullify the accessor FIRST so C-ABI callers cannot reach us mid-teardown.
    xash::abi::set_current_engine_context( nullptr );

    // Reverse declaration order:
    // server → host → map_loader → networking → clock → cmd_cvar → filesystem.
    map_loader.set_level_executor( nullptr ); // drop the dangling seam first
    server.shutdown();
    host.shutdown();
    map_loader.shutdown();
    networking.shutdown();
    platform::socket_shutdown();
    clock.shutdown();
    cmd_cvar.shutdown();
    filesystem.shutdown();
}

} // namespace xash
