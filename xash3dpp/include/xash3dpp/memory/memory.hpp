#pragma once
// xash3dpp — memory subsystem
// Legacy reference: engine/common/zone.c  (DarkPlaces-derived pool allocator)
//
// Design: stats-facade allocator.  Named pools are accounting buckets over the
// platform heap (malloc/free).  Every allocation is prefixed with an 8-byte
// header that records the pool index and payload size.  mem_free() reads back
// the header so callers do not need to pass a pool at free time.
//
// This design gives:
//   • per-pool live-byte and call-count tracking (the "memlist" command)
//   • zero bookkeeping overhead per allocation beyond the 8-byte header
//   • thread-safe counters (atomic increments/decrements)
//   • no sentinels, linked lists, or filename/line tracking — rely on ASan/MSan
//
// The poolhandle_t value space (uint32_t, 1-based index) is intentionally
// compatible with the legacy uint32_t poolhandle_t from common/xash3d_types.h
// so that renderer and physics plugin ABIs can still receive function pointers
// that satisfy the pool-flavoured signatures from ref_api_t / physint_t.
//
// @thread-safety: alloc/free/realloc and all counters are atomic-backed —
// safe from any thread; create_pool/destroy_pool/set_oom_handler are equally
// atomic but intended for the init window (see memory-boundary.md §Threading).

#include <xash3dpp/memory/stats.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace xash::memory {

// PoolHandle — a 1-based index into the pool registry.  0 (k_null_pool) is
// the invalid/untagged sentinel.
struct PoolHandle
{
    std::uint32_t index { 0 };

    [[nodiscard]] constexpr bool valid() const noexcept { return index != 0; }
    constexpr explicit operator bool() const noexcept   { return valid(); }
    constexpr bool operator==(PoolHandle o) const noexcept { return index == o.index; }
    constexpr bool operator!=(PoolHandle o) const noexcept { return index != o.index; }
};

inline constexpr PoolHandle k_null_pool {};

// ---------------------------------------------------------------------------
// Pool lifecycle
// ---------------------------------------------------------------------------

// Allocator strategy chosen at create_pool time.
// System is the only implemented strategy; the others are placeholders so
// that callers can tag intent today and the backing implementation can be
// filled in later without touching call sites.
enum class AllocStrategy : std::uint8_t
{
    System,  // malloc / free  (default)
    Arena,   // bump allocator, bulk-free on destroy_pool  (future)
    Slab,    // fixed-size object pool                     (future)
};

// Optional configuration passed to create_pool.  All fields have sensible
// defaults so existing create_pool("name") calls compile unchanged.
struct PoolConfig
{
    AllocStrategy strategy { AllocStrategy::System };
    std::size_t   reserve  { 0 };  // pre-allocation hint; currently ignored
};

// create a named pool.  Returns k_null_pool if the registry is full (> 128 pools).
[[nodiscard]] PoolHandle create_pool(const char* name, PoolConfig cfg = {}) noexcept;

// Destroy a pool slot.  In debug builds, asserts that live_bytes == 0.
// The slot is recycled for future create_pool calls.
void destroy_pool(PoolHandle handle) noexcept;

// ---------------------------------------------------------------------------
// Allocation
// ---------------------------------------------------------------------------

// Allocate size bytes tagged to pool.  Returns nullptr on OOM or size == 0.
[[nodiscard]] void* mem_alloc  (PoolHandle pool, std::size_t size)                    noexcept;

// Same as mem_alloc but zero-initialises the returned block.
[[nodiscard]] void* mem_calloc (PoolHandle pool, std::size_t size)                    noexcept;

// Resize a block.  Pool may differ from the original (migrates the tag).
// mem_realloc(pool, nullptr, n) == mem_alloc(pool, n).
// mem_realloc(pool, ptr,    0) frees ptr and returns nullptr.
[[nodiscard]] void* mem_realloc(PoolHandle pool, void* ptr, std::size_t new_size)     noexcept;

// Free a block allocated through mem_alloc/calloc/realloc.  No-op on nullptr.
// Reads the 8-byte prefix header to locate the owning pool.
void                mem_free   (void* ptr)                                             noexcept;

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

// Stats snapshot for a single pool.  Returns a zeroed struct on invalid handle.
[[nodiscard]] PoolStats  get_stats  (PoolHandle handle) noexcept;

// Number of currently active (created but not destroyed) pools.
[[nodiscard]] std::size_t pool_count() noexcept;

// Invoke fn(stats, userdata) for every active pool.  fn must not create or
// destroy pools (no re-entrancy protection is provided).
void for_each_pool(void (*fn)(PoolStats, void*), void* userdata) noexcept;

// Register a callback invoked when any allocation fails due to OOM.
// Use it to flush caches, log a diagnostic, or abort gracefully.
// Passing nullptr restores the default no-op behaviour.
// The handler must not allocate through this subsystem.
void set_oom_handler(void (*handler)(std::size_t requested, PoolHandle pool) noexcept) noexcept;

// ---------------------------------------------------------------------------
// Typed helpers
// ---------------------------------------------------------------------------

// Construct a T on the pool-backed heap.  Returns nullptr on OOM.
template<typename T, typename... Args>
[[nodiscard]] T* pool_new(PoolHandle pool, Args&&... args) noexcept
{
    // AllocHeader guarantees only 8-byte payload alignment; an over-aligned T
    // would need an aligned-alloc API plus the aligned operator-delete pair
    // (neither exists today — see architecture/memory/typed-helpers.md).
    static_assert(alignof(T) <= 8,
                  "pool_new<T>: alignof(T) > 8 unsupported — AllocHeader guarantees 8-byte payload alignment (Q-22)");
    void* p = mem_alloc(pool, sizeof(T));
    if (!p) return nullptr;
    return ::new(p) T(static_cast<Args&&>(args)...);
}

// Destroy and deallocate a T previously created with pool_new.  No-op on nullptr.
template<typename T>
void pool_delete(T* ptr) noexcept
{
    if (!ptr) return;
    ptr->~T();
    mem_free(ptr);
}

// ---------------------------------------------------------------------------
// RAII pool wrapper
// ---------------------------------------------------------------------------

class ScopedPool
{
public:
    explicit ScopedPool(const char* name) noexcept
        : handle_(create_pool(name)) {}

    ~ScopedPool() noexcept
    {
        if (handle_) destroy_pool(handle_);
    }

    ScopedPool(const ScopedPool&)            = delete;
    ScopedPool& operator=(const ScopedPool&) = delete;

    [[nodiscard]] PoolHandle handle() const noexcept       { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept  { return handle_.valid(); }

private:
    PoolHandle handle_;
};

// ---------------------------------------------------------------------------
// Typed deleter for use with std::unique_ptr when the class does NOT
// override operator delete.  Prefer adding operator delete to the class
// where possible so the standard std::unique_ptr<T> (default deleter) works.
// ---------------------------------------------------------------------------
struct PoolDeleter {
    template<typename T>
    void operator()(T* p) const noexcept { pool_delete(p); }
};

// Convenience alias: an owning pointer to a pool-allocated T.
template<typename T>
using pool_ptr = std::unique_ptr<T, PoolDeleter>;

} // namespace xash::memory
