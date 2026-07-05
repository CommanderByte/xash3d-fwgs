#pragma once
// xash3dpp — memory pool stats snapshot
//
// PoolStats lives in its own header so debug/profiling consumers can include
// just the stats view without dragging in the full pool API.
//
// @thread-safety: POD snapshot value type — safe to copy across threads.

#include <cstddef>

namespace xash::memory {

// Snapshot of pool accounting at a point in time.
struct PoolStats
{
    const char*  name         { nullptr };
    std::size_t  live_bytes   { 0 };   // bytes currently in flight
    std::size_t  total_allocs { 0 };   // cumulative allocation count
    std::size_t  total_frees  { 0 };   // cumulative free count
};

} // namespace xash::memory
