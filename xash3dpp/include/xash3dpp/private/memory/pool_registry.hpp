#pragma once
// xash3dpp — memory subsystem internals
// Exposed in a separate header (rather than buried in memory.cpp) so that
// tests can inspect pool bucket state directly without going through the
// public API, and so that future .cpp splits share the same struct layout.

#include <xash3dpp/limits.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace xash::memory::internal {

// Prepended to every allocation made through mem_alloc / mem_calloc / mem_realloc.
// The caller receives a pointer to the byte immediately after this header.
// Layout on all platforms: 8 bytes, 4-byte aligned.
struct AllocHeader
{
    std::uint32_t pool_index;   // 1-based PoolHandle::index (0 = untagged)
    std::uint32_t payload_size; // payload bytes, exclusive of this header
};
static_assert(sizeof(AllocHeader)  == 8);
static_assert(alignof(AllocHeader) == 4);

// Lifecycle state for a pool slot.
//
// Transition protocol (enforced by create_pool / destroy_pool):
//   Free   --CAS(acquire) --> Busy    create_pool atomically claims the slot
//   Busy   --store(release)--> Active create_pool publishes all field writes
//   Active --store(release)--> Free   destroy_pool releases the slot
//
// Readers (mem_alloc, mem_free, get_stats, for_each_pool) acquire-load the
// state field.  An acquire load of Active pairs with create_pool's release
// store, guaranteeing visibility of name, do_alloc, do_free, do_realloc, and
// ctx written during the Busy phase.
enum class SlotState : std::uint8_t { Free = 0, Busy = 1, Active = 2 };

// One accounting bucket per named pool.
struct PoolBucket
{
    char                     name[::xash::limits::memory_pool_name_len] {};
    std::atomic<std::size_t> live_bytes     { 0 };
    std::atomic<std::size_t> total_allocs   { 0 };
    std::atomic<std::size_t> total_frees    { 0 };
    std::atomic<SlotState>   state          { SlotState::Free };

    // Backing allocator strategy — set by create_pool, cleared by destroy_pool.
    // Null pointers fall back to the system allocator (malloc/free).
    // do_realloc may be null for strategies that don't support in-place growth;
    // mem_realloc will fall back to alloc+copy+free in that case.
    void* (*do_alloc  )(std::size_t size,                void* ctx) noexcept = nullptr;
    void  (*do_free   )(void*       ptr,                 void* ctx) noexcept = nullptr;
    void* (*do_realloc)(void*       ptr, std::size_t sz, void* ctx) noexcept = nullptr;
    void*                                                            ctx      = nullptr;  // @lifetime: strategy backend — set/cleared with the do_* pointers by create_pool/destroy_pool
};

inline constexpr std::uint32_t kMaxPools = static_cast<std::uint32_t>(xash::limits::memory_pool_max);

// Allocation-failure injection — a TEST SEAM, null in production.
//
// When installed, mem_alloc / mem_calloc / mem_realloc consult it before
// allocating and return nullptr (through the normal OOM path, so the
// oom_handler still fires) whenever it answers true.  It lives here rather
// than in the public memory.hpp for the same reason PoolBucket does: it is an
// internal affordance for the test suite, not part of the subsystem contract.
//
// It exists because the engine's out-of-memory paths were otherwise
// unreachable from a test — every pool falls back to malloc, which does not
// fail on demand.  That gap is why the 2026-07-20 modernization audit found a
// live invariant break on one of them (snapshot_alloc_ring published a buffer
// element count before the buffer existed, leaving a null pointer with a
// non-zero count for find_best_baseline to index).  Cost in production is one
// relaxed atomic load per allocation on a perfectly-predicted branch.
using AllocFailureHook = bool (*)(std::size_t size, std::uint32_t pool_index) noexcept;

// Install (or clear, with nullptr) the hook.  Returns the previous one so a
// test can restore it — always restore, the hook is process-global.
AllocFailureHook set_alloc_failure_hook(AllocFailureHook hook) noexcept;

} // namespace xash::memory::internal
