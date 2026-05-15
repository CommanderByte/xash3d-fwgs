// xash3dpp — Host implementation
// Legacy reference: engine/common/host.c

#include <xash3dpp/host/host.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>

#include <cassert>
#include <chrono>
#include <cstdio>

namespace xash {

using namespace xash::memory;
using namespace xash::filesystem;

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct Host::Impl
{
    HostArgs    args;
    HostStatus  status     = HostStatus::kInit;
    double      realtime   = 0.0;
    int         framecount = 0;

    // Subsystems owned by the host.
    // NOTE: pool is the first subsystem created and the last destroyed.
    // Impl itself is system-allocated (make_unique) because the pool does
    // not exist yet when Impl is constructed.
    PoolHandle pool;
    Filesystem fs;
};

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

Host::Host()  : impl_{ std::make_unique<Impl>() } {}
Host::~Host() = default;

// ---------------------------------------------------------------------------
// Internal: monotonic clock
// ---------------------------------------------------------------------------

static double steady_seconds() noexcept
{
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point k_start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - k_start).count();
}

// ---------------------------------------------------------------------------
// Internal: teardown (called from Main after the frame loop exits, and from
// the destructor if Init was called but Main was not used).
// ---------------------------------------------------------------------------

static void do_shutdown(Host::Impl& s) noexcept
{
    if (!s.pool) return;   // already shut down or never initialised

    // TODO: Client::Shutdown()   (non-dedicated)
    // TODO: Server::Shutdown()
    // TODO: Networking::Shutdown()
    // TODO: CmdCvar::Shutdown()
    // TODO: Platform::Shutdown()

    // Filesystem must be shut down before the host pool is released,
    // because future migrations will route FS allocations through this pool.
    s.fs.Shutdown();

    // All pool-tracked allocations are gone.  Release the pool slot.
    // (Host::Impl is system-allocated and is not tracked by this pool.)
    destroy_pool(s.pool);
    s.pool = kNullPool;

    s.status = HostStatus::kShutdown;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

bool Host::Init(const HostArgs& args)
{
    Impl& s = *impl_;
    s.args   = args;
    s.status = HostStatus::kInit;

    // --- Memory ----------------------------------------------------------
    // Create the host pool first.  All subsequent long-lived allocations
    // that have been migrated to the memory subsystem are tracked here.
    s.pool = create_pool("host");
    if (!s.pool)
    {
        std::fputs("xash3dpp host: create_pool(\"host\") failed (registry full)\n",
                   stderr);
        return false;
    }

    // --- Platform --------------------------------------------------------
    // TODO: Platform::Init(args.dedicated)

    // --- Filesystem ------------------------------------------------------
    const std::string_view rootdir = args.rootdir;
    const std::string_view basedir = args.basedir.empty() ? args.gamedir
                                                           : args.basedir;
    const std::string_view gamedir = args.gamedir.empty() ? args.basedir
                                                           : args.gamedir;
    const std::string_view rodir   = args.rodir;

    if (!s.fs.Init(rootdir, basedir, gamedir, rodir))
    {
        std::fputs("xash3dpp host: Filesystem::Init failed\n", stderr);
        destroy_pool(s.pool);
        s.pool = kNullPool;
        return false;
    }

    // Scan game directories and activate the requested game.  Non-fatal:
    // if gamedir is not found we continue in base-only mode.
    const SearchPathFlags mount_flags = SearchPathFlags::MountHD
                                      | SearchPathFlags::MountLV;

    if (!gamedir.empty() && !s.fs.ActivateGame(gamedir, mount_flags))
    {
        std::fprintf(stderr,
            "xash3dpp host: game directory '%.*s' not found, "
            "running in base mode\n",
            static_cast<int>(gamedir.size()), gamedir.data());
    }

    // --- Cmd / Cvar ------------------------------------------------------
    // TODO: CmdCvar::Init()

    // --- Networking ------------------------------------------------------
    // TODO: Networking::Init()

    // --- Server ----------------------------------------------------------
    // TODO: Server::Init()

    // --- Client (non-dedicated only) -------------------------------------
    // TODO: if (!args.dedicated) Client::Init()

    if (args.developer > 0)
    {
        std::fprintf(stdout,
            "xash3dpp: host init complete  rootdir='%s'  game='%.*s'  "
            "dedicated=%d  developer=%d\n",
            args.rootdir.c_str(),
            static_cast<int>(gamedir.size()), gamedir.data(),
            static_cast<int>(args.dedicated),
            args.developer);
    }

    s.status = HostStatus::kRunning;
    return true;
}

// ---------------------------------------------------------------------------
// RunFrame
// ---------------------------------------------------------------------------

void Host::RunFrame()
{
    Impl& s = *impl_;
    if (s.status == HostStatus::kShutdown) return;

    s.realtime = steady_seconds();
    s.framecount++;

    // TODO: Platform::PollEvents()
    // TODO: CmdCvar::ExecuteCommandBuffer()
    // TODO: Server::RunFrame()
    // TODO: Client::RunFrame()   (non-dedicated)
}

// ---------------------------------------------------------------------------
// RequestShutdown
// ---------------------------------------------------------------------------

void Host::RequestShutdown(const char* /*reason*/) noexcept
{
    impl_->status = HostStatus::kShutdown;
}

// ---------------------------------------------------------------------------
// Main — full lifecycle in one call
// ---------------------------------------------------------------------------

int Host::Main(const HostArgs& args)
{
    if (!Init(args)) return 1;

    while (impl_->status != HostStatus::kShutdown)
        RunFrame();

    do_shutdown(*impl_);
    return 0;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

HostStatus Host::status()    const noexcept { return impl_->status; }
bool       Host::dedicated() const noexcept { return impl_->args.dedicated; }
double     Host::realtime()  const noexcept { return impl_->realtime; }

} // namespace xash
