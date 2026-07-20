#pragma once
// xash3dpp — input lifetime counters and instrumentation
// @thread-safety: whole-struct relaxed atomics (any-thread read-safe),
// mirroring the NetworkingStats model (docs/design/debug-stats-design.md).
// Written only from T_Main (input's sole thread); read from any thread.
//
// Three-tier instrumentation:
//   Tier 1 (always-on): single relaxed atomics on hot-path events.
//   Tier 2 (XASH_STATS): peak/high-water bookkeeping.
//   Tier 3 (XASH_DEBUG_INPUT): per-event trace logs (not yet defined).
//
// Never gate counter increments on a runtime boolean; always measure.

#include <atomic>
#include <cstdint>

namespace xash::input {

struct InputStats
{
    // ---- Tier 1 — always-on ---------------------------------------------
    std::atomic<std::uint64_t> key_events_routed    { 0 };
    std::atomic<std::uint64_t> mouse_events_routed  { 0 };
    std::atomic<std::uint64_t> touch_events_routed  { 0 };
    std::atomic<std::uint64_t> joy_axis_events      { 0 };
    std::atomic<std::uint64_t> gyro_samples         { 0 };
    std::atomic<std::uint64_t> commands_dispatched  { 0 }; // Key_AddKeyCommands Cbuf_AddText calls

#if XASH_STATS
    // ---- Tier 2 — bookkeeping (profiling builds) ------------------------
    std::atomic<std::uint64_t> autorepeats_suppressed { 0 };
    std::atomic<std::uint64_t> unbound_key_warnings    { 0 };
#endif

#if XASH_DEBUG_INPUT
    // ---- Tier 3 — dev-only heavy tracing (not yet defined) --------------
#endif
};

} // namespace xash::input
