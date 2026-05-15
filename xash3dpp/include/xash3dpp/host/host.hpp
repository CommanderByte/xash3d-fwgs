#pragma once
// xash3dpp — Host: engine lifecycle coordinator
// Legacy reference: engine/common/host.c  (Host_Main, Host_InitCommon,
//                                          Host_Frame, Host_Shutdown,
//                                          host_parm_t / host_status_t)
//
// Responsibilities:
//   • Parse HostArgs into each subsystem's Init() parameters.
//   • Create and own the "host" memory pool.
//   • Sequence subsystem Init / Shutdown in the correct dependency order.
//   • Drive the main frame loop.
//
// What it does NOT do:
//   • Command-line argument parsing (that is the launcher's job).
//   • Platform window management, sound, input (future subsystems).

#include <memory>
#include <string>

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
//   host.Init(args);
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
    [[nodiscard]] bool Init(const HostArgs& args);
    void               RunFrame();
    void               RequestShutdown(const char* reason = nullptr) noexcept;

    // Observers
    [[nodiscard]] HostStatus status()    const noexcept;
    [[nodiscard]] bool       dedicated() const noexcept;
    [[nodiscard]] double     realtime()  const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash
