#pragma once
// xash3dpp — core::Clock: monotonic frame-timing service
// Legacy reference: engine/common/host.c
//   (Host_FilterTime, Host_CalcFPS, Host_CalcSleep, Host_Autosleep,
//    host.realtime / host.frametime / host.realframetime / host.framecount)
//
// Decision ref: docs/boundaries/host-boundary.md  Resolved-decision OQ-3
//
// Owns:
//   • realtime, frametime, realframetime, framecount, starttime, pureframetime
//   • timing cvars (host_maxfps, fps_override, host_framerate, host_sleeptime,
//     host_sleeptime_debug, sys_timescale, sys_ticrate) — registered via
//     CmdCvarContext at init() time.
//
// Threading: Clock is read by the renderer thread (Chunk 10) without locking;
// the four time fields are std::atomic<double> so concurrent reads are safe.
// Only the main thread writes (via tick()).

#include <atomic>
#include <cstdint>
#include <memory>

namespace xash::cmd_cvar { class CmdCvarContext; }

namespace xash::core {

struct ClockInitParams
{
    // Required: cvar registry the clock registers its timing cvars on.
    // Non-owning; must outlive the Clock.
    cmd_cvar::CmdCvarContext *cmd_cvar = nullptr;

    // Server mode flag — selects the FPS gate policy used in tick():
    //   true  → sys_ticrate governs frame rate  (legacy: Host_CalcFPS dedicated branch)
    //   false → host_maxfps / fps_override used  (legacy: Host_CalcFPS client branch)
    bool dedicated = false;
};

class Clock
{
public:
    Clock() noexcept;
    ~Clock();

    Clock(const Clock &)            = delete;
    Clock &operator=(const Clock &) = delete;

    Clock(Clock &&) noexcept;
    Clock &operator=(Clock &&) noexcept;

    // Initialise the clock: capture epoch, register timing cvars.
    // Returns false if cmd_cvar registration fails.
    [[nodiscard]] bool init( const ClockInitParams &p ) noexcept;

    // Release timing cvars and reset all counters.  Idempotent.
    void shutdown() noexcept;

    // Advance the clock by one frame.  Reads platform::get_time(), updates
    // realtime / frametime / realframetime / framecount applying the
    // host_framerate / sys_timescale / clamp / autosleep policy.
    //
    // Returns false if the frame budget (host_maxfps / fps_override) has not
    // yet been reached — caller should sleep and try again next iteration.
    [[nodiscard]] bool tick() noexcept;

    // Inject the singleplayer-no-demo gate used by host_framerate.
    // Decision ref: host-boundary.md Resolved-decision OQ-11
    //
    // Called post-init by Server::init() (Chunk 5) when the server subsystem
    // becomes available.  Until then the gate is nullptr and host_framerate
    // has no effect (correct for dedicated-server milestone and all tests).
    //
    // |fn| must remain valid for the life of the Clock.
    void set_frame_rate_gate( bool (*fn)() noexcept ) noexcept;

    // ---- Observers --------------------------------------------------------
    // All accessors are noexcept and may be called from any thread.

    [[nodiscard]] double        realtime()      const noexcept;
    [[nodiscard]] double        frametime()     const noexcept;
    [[nodiscard]] double        realframetime() const noexcept;
    [[nodiscard]] double        pureframetime() const noexcept;
    [[nodiscard]] double        starttime()     const noexcept;
    [[nodiscard]] std::uint64_t framecount()    const noexcept;

    // Forward declaration is public so file-scope helpers in clock.cpp may
    // take `const Impl&` parameters.  The definition stays in clock.cpp so
    // the pimpl encapsulation is preserved (Impl remains incomplete here).
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::core
