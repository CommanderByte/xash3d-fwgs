#pragma once
// xash3dpp — memory subsystem internals
// Exposed in a separate header (rather than buried in memory.cpp) so that
// tests can inspect pool bucket state directly without going through the
// public API, and so that future .cpp splits share the same struct layout.

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
//   kFree   --CAS(acquire) --> kBusy    create_pool atomically claims the slot
//   kBusy   --store(release)--> kActive create_pool publishes all field writes
//   kActive --store(release)--> kFree   destroy_pool releases the slot
//
// Readers (mem_alloc, mem_free, get_stats, for_each_pool) acquire-load the
// state field.  An acquire load of kActive pairs with create_pool's release
// store, guaranteeing visibility of name, do_alloc, do_free, do_realloc, and
// ctx written during the kBusy phase.
enum class SlotState : std::uint8_t { kFree = 0, kBusy = 1, kActive = 2 };

// One accounting bucket per named pool.
struct PoolBucket
{
    char                     name[64]       {};
    std::atomic<std::size_t> live_bytes     { 0 };
    std::atomic<std::size_t> total_allocs   { 0 };
    std::atomic<std::size_t> total_frees    { 0 };
    std::atomic<SlotState>   state          { SlotState::kFree };

    // Backing allocator strategy — set by create_pool, cleared by destroy_pool.
    // Null pointers fall back to the system allocator (malloc/free).
    // do_realloc may be null for strategies that don't support in-place growth;
    // mem_realloc will fall back to alloc+copy+free in that case.
    void* (*do_alloc  )(std::size_t size,                void* ctx) noexcept = nullptr;
    void  (*do_free   )(void*       ptr,                 void* ctx) noexcept = nullptr;
    void* (*do_realloc)(void*       ptr, std::size_t sz, void* ctx) noexcept = nullptr;
    void*                                                            ctx      = nullptr;
};

inline constexpr std::uint32_t kMaxPools = 128;

} // namespace xash::memory::internal
