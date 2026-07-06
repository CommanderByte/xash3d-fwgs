#pragma once
// xash3dpp — networking lifetime counters and instrumentation
// @thread-safety: Tier-1 counters are relaxed atomics (any-thread read-safe); Tier-2/3 written only on T_NetIO
//
// Three-tier instrumentation per docs/design/debug-stats-design.md:
//   Tier 1 (always-on): single relaxed atomics on hot-path events.
//   Tier 2 (XASH_STATS): peak/high-water bookkeeping, dropped per-cause.
//   Tier 3 (XASH_DEBUG_NETWORKING): per-packet trace logs, fragment histograms.
//
// Never gate counter increments on a runtime boolean; always measure.

#include <atomic>
#include <cstdint>

namespace xash::networking {

struct NetworkingStats
{
    // ---- Tier 1 — always-on ---------------------------------------------
    std::atomic<std::uint64_t> packets_sent     { 0 };
    std::atomic<std::uint64_t> packets_received { 0 };
    std::atomic<std::uint64_t> bytes_sent       { 0 };
    std::atomic<std::uint64_t> bytes_received   { 0 };

#if XASH_STATS
    // ---- Tier 2 — bookkeeping (profiling builds) ------------------------
    std::atomic<std::uint64_t> fragments_sent     { 0 };
    std::atomic<std::uint64_t> fragments_received { 0 };
    std::atomic<std::uint64_t> packets_dropped_overflow   { 0 };
    std::atomic<std::uint64_t> packets_dropped_invalid    { 0 };
    std::uint32_t              peak_loopback_depth        { 0 };
    std::uint32_t              peak_inflight_fragments    { 0 };
#endif

#if XASH_DEBUG_NETWORKING
    // ---- Tier 3 — dev-only heavy tracing (not yet defined) --------------
#endif
};

} // namespace xash::networking
