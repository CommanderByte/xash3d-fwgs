#pragma once
// xash3dpp — Host: engine lifecycle coordinator
// Legacy reference: engine/common/host.c  (Host_Main, Host_InitCommon,
//                                          Host_Frame, Host_Shutdown,
//                                          host_parm_t / host_status_t)
//
// Responsibilities:
//   • Parse HostArgs into each subsystem's init() parameters.
//   • create and own the "host" memory pool.
//   • Sequence subsystem init / shutdown in the correct dependency order.
//   • Drive the main frame loop.
//
// What it does NOT do:
//   • Command-line argument parsing (that is the launcher's job).
//   • Platform window management, sound, input (future subsystems).

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace xash::core       { enum class ErrorCode : std::uint32_t; }
namespace xash::core       { class Clock; }
namespace xash::cmd_cvar   { class CmdCvarContext; }
namespace xash::filesystem { class Filesystem; }
namespace xash             { class MapLoader; }

namespace xash {

// ---------------------------------------------------------------------------
// HostArgs — everything the launcher knows about the requested configuration.
//
// All string fields are owned copies; the host may keep them for its
// lifetime so the launcher does not need to keep the originals alive.
// ---------------------------------------------------------------------------
struct HostArgs
{
    int    argc      = 0;
    char** argv      = nullptr;

    // Filesystem roots
    std::string rootdir;   // engine install directory (parent of game folders)
    std::string basedir;   // always-mounted base game folder (e.g. "valve")
    std::string gamedir;   // active game folder (e.g. "cstrike"); empty = basedir
    std::string rodir;     // read-only content mirror (empty = disabled)

    // Mode flags
    bool dedicated = false;  // true when -dedicated was passed
    int  developer = 0;      // verbosity: 0 = normal, 1 = verbose, 2 = extended

    // GoldSrc bug-compatibility bitfield (Quirk Q-11, Resolved-decision OQ-7).
    // Parsed once from `-bugcomp peoei,gsmrf,sp_attn_none,...` by the launcher.
    // Subsystems read `engine_context.bugcomp & BUGCOMP_X` at the point of
    // behaviour divergence; there is no central dispatcher.
    std::uint32_t bugcomp = 0;
};

// ---------------------------------------------------------------------------
// HostInitParams — init-time configuration + injected dependencies.
//
// Decision ref: decisions-architecture.md §DI_PARAMS (Q-4)
//
// String fields are non-owning views; caller owns storage for the duration of
// the init() call — values are copied into the subsystems before returning.
//
// Injected dependency pointers are non-owning and nullable:
//   nullptr = standalone / test mode — Host skips that subsystem's
//             participation (cvar registration, frame dispatch, etc.).
// ---------------------------------------------------------------------------
struct HostInitParams
{
    // Filesystem roots
    std::string_view rootdir;   // engine install directory
    std::string_view basedir;   // always-mounted base game folder (e.g. "valve")
    std::string_view gamedir;   // active game folder (e.g. "cstrike"); empty = basedir
    std::string_view rodir;     // read-only content mirror; empty = disabled

    // Mode flags
    bool          dedicated = false;  // true when -dedicated was passed
    int           developer = 0;      // verbosity: 0 = normal, 1 = verbose, 2 = extended

    // GoldSrc bug-compatibility bitfield (Resolved-decision OQ-7).
    std::uint32_t bugcomp = 0;

    // Injected deps — non-owning; must outlive Host.
    // nullptr = Host operates in standalone / test mode for that subsystem.
    cmd_cvar::CmdCvarContext  *cmd_cvar   = nullptr;
    core::Clock               *clock      = nullptr;
    MapLoader                 *map_loader = nullptr;
    filesystem::Filesystem    *filesystem = nullptr;
};

// ---------------------------------------------------------------------------
// HostStatus — coarse lifecycle state.
// ---------------------------------------------------------------------------
enum class HostStatus
{
    kInit,       // engine is starting up
    kRunning,    // normal frame loop
    kSleep,      // minimised / background (client only, future)
    kShutdown,   // cleanup in progress or completed
};

// ---------------------------------------------------------------------------
// Host — engine singleton by convention.
//
// Typical usage (from a launcher):
//
//   xash::HostArgs args = parse_cmdline(argc, argv);
//   xash::Host host;
//   return host.Main(args);          // blocks until shutdown
//
// For embedding (Android JNI, test harness):
//
//   xash::Host host;
//   host.init(args);
//   while (host.status() == xash::HostStatus::kRunning)
//       host.RunFrame();
// ---------------------------------------------------------------------------
class Host
{
public:
    Host();
    ~Host();

    Host(const Host&)            = delete;
    Host& operator=(const Host&) = delete;

    // Full lifecycle in one call.  Returns 0 on clean shutdown, 1 on error.
    [[nodiscard]] int Main(const HostArgs& args);

    // Granular control for embedding scenarios.
    // Use HostInitParams to inject dependencies from an EngineContext.
    [[nodiscard]] bool init(const HostInitParams& p);
    void               RunFrame();
    void               shutdown() noexcept;     // explicit teardown; called by EngineContext::shutdown()
    void               RequestShutdown(const char* reason = nullptr) noexcept;

    // Frame-abort signalling (Quirk Q-3, Resolved-decision OQ-1).
    // Engine-internal callers and the GAME_EXPORT Host_Error ABI shim invoke
    // this to mark the current frame as aborted.  Recovery (SV_Shutdown /
    // CL_Drop / etc.) runs at the top of the next RunFrame.  No setjmp/longjmp.
    //
    // |code|   — typed reason; appears in diagnostics and frame_abort_code().
    // |detail| — short, non-owning view; copied into a fixed buffer (no heap).
    void signal_frame_abort(core::ErrorCode code,
                            std::string_view detail) noexcept;

    // Observers
    [[nodiscard]] HostStatus       status()             const noexcept;
    [[nodiscard]] bool             dedicated()          const noexcept;
    [[nodiscard]] double           realtime()           const noexcept;
    [[nodiscard]] bool             frame_abort_pending() const noexcept;
    [[nodiscard]] core::ErrorCode  frame_abort_code()   const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash
