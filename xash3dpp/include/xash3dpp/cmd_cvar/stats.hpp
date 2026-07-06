#pragma once
// xash3dpp — cmd_cvar lifetime counters
//
// CmdCvarStats lives in its own header so consumers that only need the stats
// view (profiling/debug tooling) do not pull in the full CmdCvarContext API.
//
// @thread-safety: all counters are std::atomic (or written game-thread-only),
// so a CmdCvarStats snapshot may be read from any thread without locking. All
// writes happen on the main (game) thread.

#include <atomic>
#include <cstdint>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// CmdCvarStats — lifetime counters exposed for profiling / debug tooling.
// The reference returned by CmdCvarContext::stats() is valid for the
// lifetime of the context.
// ---------------------------------------------------------------------------

struct CmdCvarStats {
    std::atomic<std::uint64_t> cvars_written     { 0 };     // total cvar value writes (always-on, ≤1 relaxed atomic/event)
#if XASH_STATS
    std::atomic<std::uint64_t> commands_executed { 0 };     // total dispatched commands
    std::atomic<std::uint64_t> commands_dropped  { 0 };     // filtered by privilege check
    std::atomic<std::uint32_t> buffer_high_water { 0 };     // peak command-queue depth (entries)
    std::uint32_t              peak_cvar_count   { 0 };     // max cvars registered at once
    std::uint32_t              peak_command_count{ 0 };     // max commands registered at once
#endif
};

} // namespace xash::cmd_cvar
