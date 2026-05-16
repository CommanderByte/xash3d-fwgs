// xash3dpp — core::Clock implementation
// Legacy reference: engine/common/host.c
//   (Host_FilterTime, Host_CalcFPS, Host_CalcSleep, Host_Autosleep)

#include <xash3dpp/core/clock.hpp>
#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/platform/platform.hpp>

#include <algorithm>
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

    // Timing cvars — registered at init(); nulled at shutdown().
    // All writes happen on the main thread only.
    cmd_cvar::Cvar *cv_maxfps        = nullptr;  // host_maxfps
    cmd_cvar::Cvar *cv_fps_override  = nullptr;  // fps_override
    cmd_cvar::Cvar *cv_framerate     = nullptr;  // host_framerate
    cmd_cvar::Cvar *cv_sleeptime     = nullptr;  // host_sleeptime
    cmd_cvar::Cvar *cv_sleeptime_dbg = nullptr;  // host_sleeptime_debug
    cmd_cvar::Cvar *cv_timescale     = nullptr;  // sys_timescale
    cmd_cvar::Cvar *cv_ticrate       = nullptr;  // sys_ticrate

    // OQ-11: singleplayer-no-demo gate for host_framerate.
    // Injected by Server::init() in Chunk 5; nullptr = gate always false.
    // @lifetime: fn must remain valid for the life of the Clock.
    bool (*gate_fn)() noexcept = nullptr;

    // Raw platform time of the last tick() call (main thread only).
    double oldtime = 0.0;

    // Scaled realtime at the last accepted frame (when tick() returned true).
    double last_frame_realtime = 0.0;

    bool dedicated   = false;
    bool initialised = false;

    // Always-on observability snapshot — updated at every tick() accept and shutdown().
    ClockStats stats_ {};
};

Clock::Clock() noexcept : impl_{ std::make_unique<Impl>() } {}
Clock::~Clock() = default;

Clock::Clock(Clock &&) noexcept            = default;
Clock &Clock::operator=(Clock &&) noexcept = default;

bool Clock::init( const ClockInitParams &p ) noexcept
{
    Impl &s = *impl_;
    if ( s.initialised ) return true;

    s.cmd_cvar  = p.cmd_cvar;
    s.dedicated = p.dedicated;

    const double now = platform::get_time();
    s.starttime_.store         ( now, std::memory_order_relaxed );
    s.realtime_.store          ( now, std::memory_order_relaxed );
    s.oldtime             = now;
    s.last_frame_realtime = now;
    s.framecount_.store( 0, std::memory_order_relaxed );

    s.stats_ = {};
    s.stats_.starttime = now;
    s.stats_.realtime  = now;

    // Register timing cvars.  Skip when cmd_cvar is null (test / standalone).
    // Legacy: Cvar_RegisterVariable calls in Host_InitCommon.
    if ( s.cmd_cvar )
    {
        using F = cmd_cvar::CvarFlags;
        auto &r = *s.cmd_cvar;
        s.cv_maxfps        = r.cvar_get_or_create( "host_maxfps",         "72",  F::FCVAR_ARCHIVE );
        s.cv_fps_override  = r.cvar_get_or_create( "fps_override",        "0",   0 );
        s.cv_framerate     = r.cvar_get_or_create( "host_framerate",      "0",   0 );
        s.cv_sleeptime     = r.cvar_get_or_create( "host_sleeptime",      "1",   F::FCVAR_ARCHIVE );
        s.cv_sleeptime_dbg = r.cvar_get_or_create( "host_sleeptime_debug","0",   0 );
        s.cv_timescale     = r.cvar_get_or_create( "sys_timescale",       "1.0", F::FCVAR_CHEAT );
        s.cv_ticrate       = r.cvar_get_or_create( "sys_ticrate",         "100", 0 );
    }

    s.initialised = true;
    return true;
}

void Clock::shutdown() noexcept
{
    Impl &s = *impl_;
    if ( !s.initialised ) return;

    // Null cvar handles before the CmdCvarContext shuts down.
    s.cv_maxfps = s.cv_fps_override = s.cv_framerate = nullptr;
    s.cv_sleeptime = s.cv_sleeptime_dbg = s.cv_timescale = s.cv_ticrate = nullptr;
    s.cmd_cvar = nullptr;

    s.realtime_.store     ( 0.0, std::memory_order_relaxed );
    s.frametime_.store    ( 0.0, std::memory_order_relaxed );
    s.realframetime_.store( 0.0, std::memory_order_relaxed );
    s.pureframetime_.store( 0.0, std::memory_order_relaxed );
    s.framecount_.store   ( 0,   std::memory_order_relaxed );
    s.stats_ = {};
    s.initialised = false;
}

// ---------------------------------------------------------------------------
// set_frame_rate_gate — OQ-11 post-init injection point
// ---------------------------------------------------------------------------

void Clock::set_frame_rate_gate( bool (*fn)() noexcept ) noexcept
{
    impl_->gate_fn = fn;
}

// ---------------------------------------------------------------------------
// tick() helpers — port of Host_CalcFPS from engine/common/host.c
// ---------------------------------------------------------------------------

// Returns the target FPS for the current session mode.
// Dedicated: sys_ticrate.  Client: host_maxfps with fps_override policy.
// Chunk 9 will refine the client branch with gl_vsync / demo-playback checks.
static double calc_fps( const Clock::Impl &s ) noexcept
{
    if ( s.dedicated )
    {
        // Legacy: Host_CalcFPS dedicated branch
        return s.cv_ticrate ? static_cast<double>( s.cv_ticrate->abi.value ) : 100.0;
    }

    // Legacy: Host_CalcFPS client branch (gl_vsync == 0 assumed until Chunk 9)
    const double maxfps = s.cv_maxfps ? static_cast<double>( s.cv_maxfps->abi.value ) : 72.0;

    if ( s.cv_fps_override && s.cv_fps_override->abi.value != 0.0f )
    {
        // fps_override: hard ceiling (legacy MAX_FPS_HARD = 1000)
        const double fps = ( maxfps == 0.0 ) ? limits::max_fps_hard : maxfps;
        return std::clamp( fps, limits::min_fps, limits::max_fps_hard );
    }

    // Normal client: soft ceiling (legacy MAX_FPS_SOFT = 200)
    if ( maxfps == 0.0 ) return 0.0;  // 0 = uncapped
    return std::clamp( maxfps, limits::min_fps, limits::max_fps_soft );
}

// ---------------------------------------------------------------------------
// tick() — port of Host_FilterTime / Host_Autosleep from engine/common/host.c
// ---------------------------------------------------------------------------

bool Clock::tick() noexcept
{
    Impl &s = *impl_;
    XASH_ASSERT( s.initialised );

    const double now    = platform::get_time();
    const double raw_dt = now - s.oldtime;
    s.oldtime           = now;

    const double scale = s.cv_timescale
        ? static_cast<double>( s.cv_timescale->abi.value )
        : 1.0;

    // Accumulate scaled realtime — legacy: host.realtime += time * scale
    const double new_realtime = s.realtime_.load( std::memory_order_relaxed ) + raw_dt * scale;
    s.realtime_.store( new_realtime, std::memory_order_relaxed );

    const double elapsed = new_realtime - s.last_frame_realtime;

    // --- FPS gating (legacy: Host_Autosleep) --------------------------------
    const double fps = calc_fps( s );
    if ( fps > 0.0 )
    {
        const double bounded_fps = std::clamp( fps, limits::min_fps, limits::max_fps_hard );
        // Dedicated adds +1 fps to target to avoid being fractionally early;
        // this matches the legacy dedicated-server behaviour.
        const double target_ft = s.dedicated
            ? ( 1.0 / ( bounded_fps + 1.0 ) )
            : ( 1.0 / bounded_fps );

        if ( elapsed < target_ft * scale )
        {
            // Frame budget not reached; sleep to avoid busy-spinning.
            const int sleep_ms = s.cv_sleeptime
                ? static_cast<int>( s.cv_sleeptime->abi.value ) : 1;
            if ( sleep_ms > 0 )
                platform::sleep( static_cast<unsigned>( sleep_ms ) );
            return false;
        }
    }

    // --- Frame accepted ------------------------------------------------------
    const double frame_dt = new_realtime - s.last_frame_realtime;
    s.last_frame_realtime = new_realtime;

    s.realframetime_.store(
        std::clamp( frame_dt, limits::min_frametime, limits::max_frametime ),
        std::memory_order_relaxed );

    // pureframetime: raw (unscaled) wall-clock time this frame took.
    // Used by autosleep budget accounting in future refinements.
    s.pureframetime_.store(
        std::clamp( raw_dt, limits::min_frametime, limits::max_frametime ),
        std::memory_order_relaxed );

    // host_framerate override: singleplayer-no-demo only (OQ-11 gate).
    // Legacy: Host_FilterTime host_framerate block.
    double ft = frame_dt;
    if ( s.cv_framerate
         && s.cv_framerate->abi.value > 0.0f
         && s.gate_fn && s.gate_fn() )
    {
        ft = static_cast<double>( s.cv_framerate->abi.value ) * scale;
    }
    s.frametime_.store(
        std::clamp( ft, limits::min_frametime, limits::max_frametime ),
        std::memory_order_relaxed );

    s.framecount_.fetch_add( 1, std::memory_order_relaxed );

    // Sync always-on snapshot.
    s.stats_.realtime      = s.realtime_.load     ( std::memory_order_relaxed );
    s.stats_.frametime     = s.frametime_.load    ( std::memory_order_relaxed );
    s.stats_.realframetime = s.realframetime_.load( std::memory_order_relaxed );
    s.stats_.pureframetime = s.pureframetime_.load( std::memory_order_relaxed );
    s.stats_.starttime     = s.starttime_.load    ( std::memory_order_relaxed );
    s.stats_.framecount    = s.framecount_.load   ( std::memory_order_relaxed );
    return true;
}

double        Clock::realtime()      const noexcept { return impl_->realtime_.load     ( std::memory_order_relaxed ); }
double        Clock::frametime()     const noexcept { return impl_->frametime_.load    ( std::memory_order_relaxed ); }
double        Clock::realframetime() const noexcept { return impl_->realframetime_.load( std::memory_order_relaxed ); }
double        Clock::pureframetime() const noexcept { return impl_->pureframetime_.load( std::memory_order_relaxed ); }
double        Clock::starttime()     const noexcept { return impl_->starttime_.load    ( std::memory_order_relaxed ); }
std::uint64_t Clock::framecount()    const noexcept { return impl_->framecount_.load   ( std::memory_order_relaxed ); }

const ClockStats& Clock::stats() const noexcept { return impl_->stats_; }

} // namespace xash::core
