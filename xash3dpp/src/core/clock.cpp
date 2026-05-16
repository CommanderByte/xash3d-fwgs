// xash3dpp — core::Clock implementation (Chunk 3 scaffold)
// Legacy reference: engine/common/host.c (Host_FilterTime/CalcFPS/CalcSleep)
//
// Existing subsystems used:
//   xash3dpp_platform — platform::get_time() for monotonic wall clock

#include <xash3dpp/core/clock.hpp>
#include <xash3dpp/platform/platform.hpp>

#include <atomic>

namespace xash::core {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct Clock::Impl
{
    cmd_cvar::CmdCvarContext *cmd_cvar = nullptr;

    // Atomic so the renderer thread (Chunk 10) can read without locking.
    std::atomic<double>        realtime_      { 0.0 };
    std::atomic<double>        frametime_     { 0.0 };
    std::atomic<double>        realframetime_ { 0.0 };
    std::atomic<double>        pureframetime_ { 0.0 };
    std::atomic<double>        starttime_     { 0.0 };
    std::atomic<std::uint64_t> framecount_    { 0 };

    double oldtime = 0.0;
    bool   initialised = false;
};

Clock::Clock() noexcept : impl_{ std::make_unique<Impl>() } {}
Clock::~Clock() = default;

Clock::Clock(Clock &&) noexcept            = default;
Clock &Clock::operator=(Clock &&) noexcept = default;

bool Clock::init( const ClockInitParams &p ) noexcept
{
    Impl &s = *impl_;
    if (s.initialised) return true;
    s.cmd_cvar = p.cmd_cvar;

    const double now = platform::get_time();
    s.starttime_.store( now, std::memory_order_relaxed );
    s.realtime_.store ( now, std::memory_order_relaxed );
    s.oldtime          = now;
    s.framecount_.store( 0, std::memory_order_relaxed );

    // TODO Chunk 3 implementation prompt: register timing cvars via
    //   p.cmd_cvar->cvar_get_or_create("host_maxfps",       "72", FCVAR_ARCHIVE);
    //   p.cmd_cvar->cvar_get_or_create("fps_override",       "0", 0);
    //   p.cmd_cvar->cvar_get_or_create("host_framerate",     "0", 0);
    //   p.cmd_cvar->cvar_get_or_create("host_sleeptime",     "1", FCVAR_ARCHIVE);
    //   p.cmd_cvar->cvar_get_or_create("host_sleeptime_debug","0", 0);
    //   p.cmd_cvar->cvar_get_or_create("sys_timescale",      "1.0", FCVAR_CHEAT);
    //   p.cmd_cvar->cvar_get_or_create("sys_ticrate",        "100", 0);

    s.initialised = true;
    return true;
}

void Clock::shutdown() noexcept
{
    Impl &s = *impl_;
    if (!s.initialised) return;

    // TODO Chunk 3 implementation: unregister timing cvars.
    s.realtime_.store( 0.0, std::memory_order_relaxed );
    s.frametime_.store( 0.0, std::memory_order_relaxed );
    s.framecount_.store( 0, std::memory_order_relaxed );
    s.initialised = false;
}

bool Clock::tick() noexcept
{
    Impl &s = *impl_;
    const double now = platform::get_time();
    const double dt  = now - s.oldtime;
    s.oldtime        = now;

    // Minimal scaffold: no FPS gating, no host_framerate override yet.
    // The Chunk 3 implementation prompt fills these in by porting
    // Host_FilterTime / Host_CalcFPS from engine/common/host.c.
    s.realtime_.store     ( now, std::memory_order_relaxed );
    s.frametime_.store    ( dt,  std::memory_order_relaxed );
    s.realframetime_.store( dt,  std::memory_order_relaxed );
    s.framecount_.fetch_add( 1,  std::memory_order_relaxed );
    return true;
}

double        Clock::realtime()      const noexcept { return impl_->realtime_.load     ( std::memory_order_relaxed ); }
double        Clock::frametime()     const noexcept { return impl_->frametime_.load    ( std::memory_order_relaxed ); }
double        Clock::realframetime() const noexcept { return impl_->realframetime_.load( std::memory_order_relaxed ); }
double        Clock::pureframetime() const noexcept { return impl_->pureframetime_.load( std::memory_order_relaxed ); }
double        Clock::starttime()     const noexcept { return impl_->starttime_.load    ( std::memory_order_relaxed ); }
std::uint64_t Clock::framecount()    const noexcept { return impl_->framecount_.load   ( std::memory_order_relaxed ); }

} // namespace xash::core
