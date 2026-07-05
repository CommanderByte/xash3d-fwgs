// xash3dpp — memory subsystem implementation
// See include/xash3dpp/memory/memory.hpp for the design rationale.

#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/memory/pool_registry.hpp>

#include <xash3dpp/core/assert.hpp>
#include <cstdlib>  // malloc, realloc, free
#include <cstring>  // memset, strncpy

namespace xash::memory {

using namespace internal;

// ---------------------------------------------------------------------------
// System allocator wrappers — the default strategy for every pool.
// ---------------------------------------------------------------------------

static void* sys_alloc(std::size_t size, void*) noexcept
{
    return std::malloc(size);
}

static void sys_free(void* ptr, void*) noexcept
{
    std::free(ptr);
}

static void* sys_realloc(void* ptr, std::size_t size, void*) noexcept
{
    return std::realloc(ptr, size);
}

// ---------------------------------------------------------------------------
// Global pool registry — a fixed-size flat array; no heap allocation needed.
// ---------------------------------------------------------------------------

static PoolBucket g_pools[kMaxPools];

// OOM callback — null by default (callers receive nullptr and handle it).
// Stored atomically so set_oom_handler() is safe to call from any thread.
using OomHandler = void (*)(std::size_t, PoolHandle) noexcept;
static std::atomic<OomHandler> g_oom_handler { nullptr };

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static PoolBucket* bucket_of(PoolHandle h) noexcept
{
    if (!h || h.index > kMaxPools) return nullptr;
    return &g_pools[h.index - 1];
}

static AllocHeader* header_of(void* ptr) noexcept
{
    return static_cast<AllocHeader*>(ptr) - 1;
}

// ---------------------------------------------------------------------------
// Pool lifecycle
// ---------------------------------------------------------------------------

PoolHandle create_pool(const char* name, PoolConfig cfg) noexcept
{
    for (std::uint32_t i = 0; i < kMaxPools; ++i)
    {
        PoolBucket& b = g_pools[i];

        // Fast pre-check with relaxed load to skip non-free slots cheaply.
        if (b.state.load(std::memory_order_relaxed) != SlotState::Free)
            continue;

        // Atomically claim the slot.  The acquire on success synchronises with
        // the release in destroy_pool, ensuring we see its prior field clears.
        SlotState expected = SlotState::Free;
        if (!b.state.compare_exchange_strong(expected, SlotState::Busy,
                std::memory_order_acquire, std::memory_order_relaxed))
            continue;  // another thread claimed this slot first

        // We own the slot exclusively.  All writes below happen-before the
        // release store of Active, so any reader that acquire-loads Active
        // is guaranteed to see them.
        b.live_bytes.store  (0, std::memory_order_relaxed);
        b.total_allocs.store(0, std::memory_order_relaxed);
        b.total_frees.store (0, std::memory_order_relaxed);

        if (name)
        {
            std::strncpy(b.name, name, sizeof(b.name) - 1);
            b.name[sizeof(b.name) - 1] = '\0';
        }
        else
        {
            b.name[0] = '\0';
        }

        // Wire up the backing allocator.  Only System is implemented today;
        // future strategies will add their setup here without touching callers.
        switch (cfg.strategy)
        {
            case AllocStrategy::Arena:  // fall through — not yet implemented
            case AllocStrategy::Slab:   // fall through — not yet implemented
            case AllocStrategy::System:
            default:
                b.do_alloc   = sys_alloc;
                b.do_free    = sys_free;
                b.do_realloc = sys_realloc;
                b.ctx        = nullptr;
                break;
        }

        // Publish.  The release store makes all prior writes visible to any
        // thread that subsequently acquire-loads Active.
        b.state.store(SlotState::Active, std::memory_order_release);
        return PoolHandle { i + 1 };
    }

    // Registry full — caller gets k_null_pool; subsequent allocs through it are
    // untracked (they still succeed, pool_index == 0 in the header).
    return k_null_pool;
}

void destroy_pool(PoolHandle handle) noexcept
{
    PoolBucket* b = bucket_of(handle);
    if (!b) return;

    // Acquire-load ensures we see all field writes from create_pool.
    if (b->state.load(std::memory_order_acquire) != SlotState::Active)
        return;

    XASH_ASSERT(b->live_bytes.load(std::memory_order_relaxed) == 0);

    // Clear all fields before the release store.  The release store of Free
    // makes these clears visible to the next create_pool that CAS-acquires
    // this slot.
    b->name[0]    = '\0';
    b->do_alloc   = nullptr;
    b->do_free    = nullptr;
    b->do_realloc = nullptr;
    b->ctx        = nullptr;
    b->live_bytes.store  (0, std::memory_order_relaxed);
    b->total_allocs.store(0, std::memory_order_relaxed);
    b->total_frees.store (0, std::memory_order_relaxed);

    // Release the slot for reuse.
    b->state.store(SlotState::Free, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Allocation
// ---------------------------------------------------------------------------

void* mem_alloc(PoolHandle pool, std::size_t size) noexcept
{
    if (size == 0) return nullptr;

    // AllocHeader stores payload_size as uint32_t — reject requests the
    // header cannot represent (silent truncation would corrupt the free /
    // realloc accounting).  Unreachable on 32-bit targets (size_t is 32-bit).
    if (size > static_cast<std::size_t>(UINT32_MAX))
    {
        if (auto h = g_oom_handler.load(std::memory_order_acquire)) h(size, pool);
        return nullptr;
    }

    PoolBucket* b = bucket_of(pool);
    // Acquire-load the slot state.  If not Active, treat as untracked; the
    // acquire also ensures do_alloc / ctx written by create_pool are visible.
    if (b && b->state.load(std::memory_order_acquire) != SlotState::Active)
        b = nullptr;

    std::size_t raw_size = sizeof(AllocHeader) + size;
    if (raw_size <= size)  // integer overflow: size is too large
    {
        if (auto h = g_oom_handler.load(std::memory_order_acquire)) h(size, pool);
        return nullptr;
    }
    void* raw = b && b->do_alloc ? b->do_alloc(raw_size, b->ctx)
                                 : std::malloc(raw_size);
    if (!raw)
    {
        if (auto h = g_oom_handler.load(std::memory_order_acquire)) h(size, pool);
        return nullptr;
    }

    auto* hdr         = static_cast<AllocHeader*>(raw);
    hdr->pool_index   = pool.index;
    hdr->payload_size = static_cast<std::uint32_t>(size);

    if (b)
    {
        b->live_bytes.fetch_add  (size, std::memory_order_relaxed);
        b->total_allocs.fetch_add(1,    std::memory_order_relaxed);
    }

    return hdr + 1;
}

void* mem_calloc(PoolHandle pool, std::size_t size) noexcept
{
    void* p = mem_alloc(pool, size);
    if (p) std::memset(p, 0, size);
    return p;
}

void* mem_realloc(PoolHandle pool, void* ptr, std::size_t new_size) noexcept
{
    if (!ptr)      return mem_alloc(pool, new_size);
    if (!new_size) { mem_free(ptr); return nullptr; }

    // payload_size is uint32_t — same representability guard as mem_alloc.
    if (new_size > static_cast<std::size_t>(UINT32_MAX))
    {
        if (auto h = g_oom_handler.load(std::memory_order_acquire)) h(new_size, pool);
        return nullptr;  // original block still intact
    }

    // Guard against overflow in the raw-size calculation.
    {
        std::size_t check = sizeof(AllocHeader) + new_size;
        if (check <= new_size)
        {
            if (auto h = g_oom_handler.load(std::memory_order_acquire)) h(new_size, pool);
            return nullptr;
        }
    }

    AllocHeader* old_hdr  = header_of(ptr);
    std::size_t  old_size = old_hdr->payload_size;
    PoolHandle   old_pool { old_hdr->pool_index };
    PoolBucket*  old_b    = bucket_of(old_pool);
    PoolBucket*  new_b    = bucket_of(pool);

    // Acquire-load ensures function pointers are visible before we use them.
    if (old_b && old_b->state.load(std::memory_order_acquire) != SlotState::Active)
        old_b = nullptr;
    if (new_b && new_b->state.load(std::memory_order_acquire) != SlotState::Active)
        new_b = nullptr;

    const std::size_t raw_size = sizeof(AllocHeader) + new_size;
    void* raw = nullptr;

    // Fast path: same pool with a native realloc (avoids copy for System).
    if (old_pool == pool && new_b && new_b->do_realloc)
    {
        raw = new_b->do_realloc(old_hdr, raw_size, new_b->ctx);
    }
    else
    {
        // Cross-pool or strategy without native realloc: alloc + copy + free.
        raw = new_b && new_b->do_alloc ? new_b->do_alloc(raw_size, new_b->ctx)
                                       : std::malloc(raw_size);
        if (raw)
        {
            std::size_t copy_bytes = old_size < new_size ? old_size : new_size;
            std::memcpy(static_cast<char*>(raw) + sizeof(AllocHeader), ptr, copy_bytes);
            if (old_b && old_b->do_free) old_b->do_free(old_hdr, old_b->ctx);
            else                         std::free(old_hdr);
        }
    }

    if (!raw)
    {
        if (auto h = g_oom_handler.load(std::memory_order_acquire)) h(new_size, pool);
        return nullptr;  // original block still intact
    }

    auto* hdr         = static_cast<AllocHeader*>(raw);
    hdr->pool_index   = pool.index;
    hdr->payload_size = static_cast<std::uint32_t>(new_size);

    // Debit old pool, credit new pool (they may be the same).
    if (old_b)
    {
        old_b->live_bytes.fetch_sub  (old_size, std::memory_order_relaxed);
        old_b->total_frees.fetch_add (1,        std::memory_order_relaxed);
    }
    if (new_b)
    {
        new_b->live_bytes.fetch_add  (new_size, std::memory_order_relaxed);
        new_b->total_allocs.fetch_add(1,        std::memory_order_relaxed);
    }

    return hdr + 1;
}

void mem_free(void* ptr) noexcept
{
    if (!ptr) return;

    AllocHeader* hdr  = header_of(ptr);
    PoolHandle   pool { hdr->pool_index };
    std::size_t  size = hdr->payload_size;
    PoolBucket*  b    = bucket_of(pool);

    // Acquire-load ensures do_free is visible if the pool is still active.
    if (b && b->state.load(std::memory_order_acquire) != SlotState::Active)
        b = nullptr;

    if (b)
    {
        b->live_bytes.fetch_sub  (size, std::memory_order_relaxed);
        b->total_frees.fetch_add (1,    std::memory_order_relaxed);
    }

    if (b && b->do_free) b->do_free(hdr, b->ctx);
    else                 std::free(hdr);
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

PoolStats get_stats(PoolHandle handle) noexcept
{
    PoolBucket* b = bucket_of(handle);
    if (!b || b->state.load(std::memory_order_acquire) != SlotState::Active) return {};

    return PoolStats {
        b->name,
        b->live_bytes.load  (std::memory_order_relaxed),
        b->total_allocs.load(std::memory_order_relaxed),
        b->total_frees.load (std::memory_order_relaxed),
    };
}

std::size_t pool_count() noexcept
{
    std::size_t count = 0;
    for (auto& b : g_pools)
        if (b.state.load(std::memory_order_relaxed) == SlotState::Active) ++count;
    return count;
}

void for_each_pool(void (*fn)(PoolStats, void*), void* userdata) noexcept
{
    if (!fn) return;
    for (std::uint32_t i = 0; i < kMaxPools; ++i)
    {
        PoolBucket& b = g_pools[i];
        if (b.state.load(std::memory_order_acquire) != SlotState::Active) continue;
        fn(PoolStats {
            b.name,
            b.live_bytes.load  (std::memory_order_relaxed),
            b.total_allocs.load(std::memory_order_relaxed),
            b.total_frees.load (std::memory_order_relaxed),
        }, userdata);
    }
}

void set_oom_handler(void (*handler)(std::size_t, PoolHandle) noexcept) noexcept
{
    g_oom_handler.store(handler, std::memory_order_release);
}

} // namespace xash::memory
