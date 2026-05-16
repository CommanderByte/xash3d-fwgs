// xash3dpp — memory subsystem tests
// Covers: pool lifecycle, alloc/free stats tracking, calloc zero-init,
//         realloc grow/shrink/migrate, edge cases, typed helpers, ScopedPool.

#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/memory/pool_registry.hpp>

#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "../test_helpers.hpp"

using namespace xash::memory;
using namespace xash::memory::internal;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Helpers — tear down any pools created during a test so the registry is
// clean for the next one.
// ---------------------------------------------------------------------------

static void cleanup_pool(PoolHandle& h)
{
    if (h)
    {
        // drain live allocations if any were left (test sloppiness)
        destroy_pool(h);
        h = k_null_pool;
    }
}

// ---------------------------------------------------------------------------
// Pool lifecycle
// ---------------------------------------------------------------------------

static void test_create_destroy()
{
    PoolHandle h = create_pool("test_pool");
    CHECK(h.valid());
    CHECK(pool_count() >= 1);

    PoolStats s = get_stats(h);
    CHECK(s.name != nullptr);
    CHECK(std::strcmp(s.name, "test_pool") == 0);
    CHECK(s.live_bytes   == 0);
    CHECK(s.total_allocs == 0);
    CHECK(s.total_frees  == 0);

    destroy_pool(h);
    // After destruction, get_stats returns zeroed struct.
    PoolStats dead = get_stats(h);
    CHECK(dead.name == nullptr);
}

static void test_null_pool_handle()
{
    CHECK(!k_null_pool.valid());
    CHECK(k_null_pool.index == 0);

    PoolStats s = get_stats(k_null_pool);
    CHECK(s.name       == nullptr);
    CHECK(s.live_bytes == 0);

    // Destroying the null pool is a no-op (must not crash).
    destroy_pool(k_null_pool);
}

static void test_pool_count()
{
    std::size_t before = pool_count();

    PoolHandle a = create_pool("A");
    PoolHandle b = create_pool("B");
    CHECK(pool_count() == before + 2);

    destroy_pool(a);
    CHECK(pool_count() == before + 1);

    destroy_pool(b);
    CHECK(pool_count() == before);
}

static void test_slot_reuse()
{
    PoolHandle first = create_pool("slot_reuse");
    std::uint32_t idx = first.index;
    destroy_pool(first);

    // The freed slot should be reused by the next create_pool call.
    PoolHandle second = create_pool("reused");
    CHECK(second.index == idx);
    destroy_pool(second);
}

// ---------------------------------------------------------------------------
// Basic alloc / free stats
// ---------------------------------------------------------------------------

static void test_alloc_increments_stats()
{
    PoolHandle h = create_pool("alloc_stats");

    void* p = mem_alloc(h, 64);
    CHECK(p != nullptr);

    PoolStats s = get_stats(h);
    CHECK(s.live_bytes   == 64);
    CHECK(s.total_allocs == 1);
    CHECK(s.total_frees  == 0);

    mem_free(p);
    s = get_stats(h);
    CHECK(s.live_bytes   == 0);
    CHECK(s.total_allocs == 1);
    CHECK(s.total_frees  == 1);

    destroy_pool(h);
}

static void test_multiple_allocs()
{
    PoolHandle h = create_pool("multi_alloc");

    void* p1 = mem_alloc(h, 16);
    void* p2 = mem_alloc(h, 32);
    void* p3 = mem_alloc(h, 48);

    CHECK(get_stats(h).live_bytes   == 96);
    CHECK(get_stats(h).total_allocs == 3);

    mem_free(p2);
    CHECK(get_stats(h).live_bytes  == 64);
    CHECK(get_stats(h).total_frees == 1);

    mem_free(p1);
    mem_free(p3);
    CHECK(get_stats(h).live_bytes  == 0);
    CHECK(get_stats(h).total_frees == 3);

    destroy_pool(h);
}

static void test_free_nullptr_is_noop()
{
    PoolHandle h = create_pool("free_null");

    void* p = mem_alloc(h, 8);
    CHECK(get_stats(h).total_allocs == 1);

    // mem_free(nullptr) is a no-op: must not crash and must not record a free.
    mem_free(nullptr);
    CHECK(get_stats(h).total_frees == 0);

    mem_free(p);
    CHECK(get_stats(h).total_frees == 1);

    destroy_pool(h);
}

static void test_alloc_zero_size()
{
    PoolHandle h = create_pool("zero_size");
    void* p = mem_alloc(h, 0);
    CHECK(p == nullptr);
    CHECK(get_stats(h).total_allocs == 0);
    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// Untagged allocations (k_null_pool)
// ---------------------------------------------------------------------------

static void test_alloc_null_pool()
{
    // Allocating through k_null_pool is allowed — it just isn't tracked.
    void* p = mem_alloc(k_null_pool, 32);
    CHECK(p != nullptr);
    mem_free(p);  // must not crash
}

// ---------------------------------------------------------------------------
// calloc — zero initialisation
// ---------------------------------------------------------------------------

static void test_calloc_zeroes()
{
    PoolHandle h = create_pool("calloc_test");

    auto* buf = static_cast<unsigned char*>(mem_calloc(h, 128));
    CHECK(buf != nullptr);

    bool all_zero = true;
    for (int i = 0; i < 128; ++i)
        if (buf[i] != 0) { all_zero = false; break; }
    CHECK(all_zero);

    mem_free(buf);
    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// realloc
// ---------------------------------------------------------------------------

static void test_realloc_grow()
{
    PoolHandle h = create_pool("realloc_grow");

    void* p = mem_alloc(h, 32);
    CHECK(get_stats(h).live_bytes == 32);

    p = mem_realloc(h, p, 128);
    CHECK(p != nullptr);
    CHECK(get_stats(h).live_bytes == 128);

    mem_free(p);
    destroy_pool(h);
}

static void test_realloc_shrink()
{
    PoolHandle h = create_pool("realloc_shrink");

    void* p = mem_alloc(h, 256);
    p = mem_realloc(h, p, 16);
    CHECK(p != nullptr);
    CHECK(get_stats(h).live_bytes == 16);

    mem_free(p);
    destroy_pool(h);
}

static void test_realloc_nullptr_acts_as_alloc()
{
    PoolHandle h = create_pool("realloc_null_ptr");

    void* p = mem_realloc(h, nullptr, 64);
    CHECK(p != nullptr);
    CHECK(get_stats(h).live_bytes   == 64);
    CHECK(get_stats(h).total_allocs == 1);

    mem_free(p);
    destroy_pool(h);
}

static void test_realloc_zero_size_acts_as_free()
{
    PoolHandle h = create_pool("realloc_zero");

    void* p = mem_alloc(h, 64);
    void* q = mem_realloc(h, p, 0);
    CHECK(q == nullptr);
    CHECK(get_stats(h).live_bytes  == 0);
    CHECK(get_stats(h).total_frees == 1);

    destroy_pool(h);
}

static void test_realloc_pool_migration()
{
    PoolHandle src = create_pool("migrate_src");
    PoolHandle dst = create_pool("migrate_dst");

    void* p = mem_alloc(src, 64);
    CHECK(get_stats(src).live_bytes == 64);
    CHECK(get_stats(dst).live_bytes == 0);

    // Reallocate into dst pool.
    p = mem_realloc(dst, p, 128);
    CHECK(p != nullptr);
    CHECK(get_stats(src).live_bytes == 0);
    CHECK(get_stats(dst).live_bytes == 128);

    mem_free(p);
    destroy_pool(src);
    destroy_pool(dst);
}

static void test_realloc_preserves_data()
{
    PoolHandle h = create_pool("realloc_data");

    auto* p = static_cast<char*>(mem_alloc(h, 8));
    std::memcpy(p, "HELLO123", 8);

    p = static_cast<char*>(mem_realloc(h, p, 64));
    CHECK(std::memcmp(p, "HELLO123", 8) == 0);

    mem_free(p);
    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// for_each_pool
// ---------------------------------------------------------------------------

static void test_for_each_pool()
{
    PoolHandle a = create_pool("foreach_A");
    PoolHandle b = create_pool("foreach_B");

    void* pa = mem_alloc(a, 10);
    void* pb = mem_alloc(b, 20);

    struct Acc { int count; std::size_t bytes; };
    Acc acc { 0, 0 };
    for_each_pool([](PoolStats s, void* ud) {
        auto* acc = static_cast<Acc*>(ud);
        ++acc->count;
        acc->bytes += s.live_bytes;
    }, &acc);

    CHECK(acc.count >= 2);
    CHECK(acc.bytes >= 30);  // at least the 10 + 20 we allocated

    // Passing null fn must not crash.
    for_each_pool(nullptr, nullptr);

    mem_free(pa);
    mem_free(pb);
    destroy_pool(a);
    destroy_pool(b);
}

// ---------------------------------------------------------------------------
// Typed helpers — pool_new / pool_delete
// ---------------------------------------------------------------------------

struct Widget
{
    int   value;
    float scale;
    explicit Widget(int v, float s) noexcept : value(v), scale(s) {}
    ~Widget() noexcept { value = -1; }
};

static void test_pool_new_delete()
{
    PoolHandle h = create_pool("pool_new");

    Widget* w = pool_new<Widget>(h, 42, 1.5f);
    CHECK(w != nullptr);
    CHECK(w->value == 42);
    CHECK(w->scale == 1.5f);
    CHECK(get_stats(h).live_bytes == sizeof(Widget));

    pool_delete(w);
    CHECK(get_stats(h).live_bytes  == 0);
    CHECK(get_stats(h).total_frees == 1);

    destroy_pool(h);
}

static void test_pool_delete_nullptr()
{
    // Must not crash.
    pool_delete(static_cast<Widget*>(nullptr));
}

// ---------------------------------------------------------------------------
// ScopedPool RAII
// ---------------------------------------------------------------------------

static void test_scoped_pool()
{
    std::size_t before = pool_count();
    {
        ScopedPool sp("scoped");
        CHECK(sp.handle().valid());
        CHECK(bool(sp));
        CHECK(pool_count() == before + 1);
    }
    // Pool destroyed when sp goes out of scope.
    CHECK(pool_count() == before);
}

static void test_scoped_pool_handle_usable()
{
    ScopedPool sp("scoped_alloc");
    void* p = mem_alloc(sp.handle(), 100);
    CHECK(p != nullptr);
    CHECK(get_stats(sp.handle()).live_bytes == 100);
    mem_free(p);
    // ScopedPool destructor runs here (live_bytes == 0, assert passes).
}

// ---------------------------------------------------------------------------
// Registry capacity
// ---------------------------------------------------------------------------

static void test_registry_full()
{
    // Exhaust the registry then verify create_pool returns k_null_pool.
    PoolHandle handles[kMaxPools];
    std::size_t created = 0;

    for (std::uint32_t i = 0; i < kMaxPools; ++i)
    {
        handles[i] = create_pool("cap_test");
        if (handles[i].valid()) ++created;
        else break;
    }

    // If all slots were taken, next one must fail.
    if (created == kMaxPools)
    {
        PoolHandle overflow = create_pool("overflow");
        CHECK(!overflow.valid());
    }
    else
    {
        // Some slots were already occupied by other tests — that is fine.
        CHECK(created > 0);
    }

    for (std::uint32_t i = 0; i < created; ++i)
        destroy_pool(handles[i]);
}

// ---------------------------------------------------------------------------
// PoolConfig / AllocStrategy
// ---------------------------------------------------------------------------

static void test_pool_config_system_explicit()
{
    // Explicit System must behave identically to the default.
    PoolHandle h = create_pool("explicit_system", PoolConfig{ AllocStrategy::System });
    CHECK(h.valid());

    void* p = mem_alloc(h, 64);
    CHECK(p != nullptr);
    CHECK(get_stats(h).live_bytes == 64);

    mem_free(p);
    destroy_pool(h);
}

static void test_pool_config_future_strategies()
{
    // Arena and Slab are not yet implemented; they silently fall back to
    // System.  Allocations must still succeed and stats must still be tracked.
    PoolHandle arena = create_pool("future_arena", PoolConfig{ AllocStrategy::Arena });
    PoolHandle slab  = create_pool("future_slab",  PoolConfig{ AllocStrategy::Slab  });
    CHECK(arena.valid());
    CHECK(slab.valid());

    void* pa = mem_alloc(arena, 32);
    void* ps = mem_alloc(slab,  32);
    CHECK(pa != nullptr);
    CHECK(ps != nullptr);
    CHECK(get_stats(arena).live_bytes == 32);
    CHECK(get_stats(slab).live_bytes  == 32);

    mem_free(pa);
    mem_free(ps);
    destroy_pool(arena);
    destroy_pool(slab);
}

// ---------------------------------------------------------------------------
// OOM handler
// ---------------------------------------------------------------------------

static std::size_t g_oom_size = 0;
static PoolHandle  g_oom_pool = k_null_pool;

static void oom_callback(std::size_t requested, PoolHandle pool) noexcept
{
    g_oom_size = requested;
    g_oom_pool = pool;
}

static void test_oom_handler()
{
    // Register, clear, and re-register must not crash.
    set_oom_handler(oom_callback);
    set_oom_handler(nullptr);
    set_oom_handler(oom_callback);

    PoolHandle h = create_pool("oom_test");

    // (SIZE_MAX >> 1) + 1 is guaranteed to be too large for any real system
    // and won't overflow when sizeof(AllocHeader) is added to it.
    const std::size_t huge = (static_cast<std::size_t>(-1) >> 1) + 1;
    g_oom_size = 0;
    g_oom_pool = k_null_pool;

    void* p = mem_alloc(h, huge);
    CHECK(p == nullptr);
    CHECK(g_oom_size == huge);
    CHECK(g_oom_pool == h);

    // After clearing the handler, a failed alloc must not invoke it.
    set_oom_handler(nullptr);
    g_oom_size = 0;
    p = mem_alloc(h, huge);
    CHECK(p == nullptr);
    CHECK(g_oom_size == 0);  // handler was not called

    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// Pool name edge cases
// ---------------------------------------------------------------------------

static void test_pool_name_truncation()
{
    // A name longer than 63 characters must be stored truncated, not
    // overflowing the 64-byte buffer in PoolBucket.
    const char* long_name =
        "this_pool_name_is_deliberately_longer_than_sixty_three_characters_abcdef";
    CHECK(std::strlen(long_name) > 63);

    PoolHandle h = create_pool(long_name);
    CHECK(h.valid());

    PoolStats s = get_stats(h);
    CHECK(s.name != nullptr);
    CHECK(std::strlen(s.name) == 63);

    destroy_pool(h);
}

static void test_pool_name_null()
{
    // create_pool(nullptr) must not crash; the pool is usable and name is empty.
    PoolHandle h = create_pool(nullptr);
    CHECK(h.valid());

    PoolStats s = get_stats(h);
    CHECK(s.name != nullptr);
    CHECK(s.name[0] == '\0');

    void* p = mem_alloc(h, 16);
    CHECK(p != nullptr);
    mem_free(p);

    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// Allocation alignment
// ---------------------------------------------------------------------------

static void test_alloc_alignment()
{
    // The system allocator returns max_align_t-aligned memory.  After the
    // 8-byte AllocHeader prefix the payload pointer must be at least
    // 8-byte aligned (covers double, pointer, int64_t on all targets).
    PoolHandle h = create_pool("alignment");

    for (std::size_t sz : { std::size_t(1), std::size_t(7), std::size_t(8),
                            std::size_t(15), std::size_t(128) })
    {
        void* p = mem_alloc(h, sz);
        CHECK(p != nullptr);
        CHECK((reinterpret_cast<std::uintptr_t>(p) % 8) == 0);
        mem_free(p);
    }

    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// Realloc — same-pool alloc/free counter accuracy
// ---------------------------------------------------------------------------

static void test_realloc_same_pool_counts()
{
    // Each realloc counts as one implicit free of the old size
    // and one new alloc of the new size.
    PoolHandle h = create_pool("realloc_counts");

    void* p = mem_alloc(h, 16);
    CHECK(get_stats(h).total_allocs == 1);
    CHECK(get_stats(h).total_frees  == 0);

    p = mem_realloc(h, p, 64);
    CHECK(p != nullptr);
    CHECK(get_stats(h).total_allocs == 2);
    CHECK(get_stats(h).total_frees  == 1);
    CHECK(get_stats(h).live_bytes   == 64);

    p = mem_realloc(h, p, 8);
    CHECK(get_stats(h).total_allocs == 3);
    CHECK(get_stats(h).total_frees  == 2);
    CHECK(get_stats(h).live_bytes   == 8);

    mem_free(p);
    CHECK(get_stats(h).total_frees == 3);
    CHECK(get_stats(h).live_bytes  == 0);

    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// Overflow guard
// ---------------------------------------------------------------------------

static void test_alloc_overflow_guard()
{
    // Requesting a size that would overflow sizeof(AllocHeader) + size must
    // return nullptr rather than silently allocating a tiny block.
    PoolHandle h = create_pool("overflow_guard");

    // SIZE_MAX itself overflows when the 8-byte header is added.
    void* p = mem_alloc(h, static_cast<std::size_t>(-1));
    CHECK(p == nullptr);
    CHECK(get_stats(h).total_allocs == 0);  // no allocation was recorded

    // SIZE_MAX - 7 also overflows (8 + (SIZE_MAX-7) == SIZE_MAX+1 wraps to 0).
    p = mem_alloc(h, static_cast<std::size_t>(-1) - 7);
    CHECK(p == nullptr);

    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// Thread safety
// ---------------------------------------------------------------------------

static void test_concurrent_create_pool()
{
    // N threads each call create_pool concurrently.  Every handle must be
    // valid and no two handles may share the same slot index.
    constexpr int N = 8;
    PoolHandle handles[N] {};

    std::vector<std::thread> threads;
    threads.reserve(N);
    for (int i = 0; i < N; ++i)
        threads.emplace_back([&handles, i]{ handles[i] = create_pool("concurrent"); });
    for (auto& t : threads) t.join();

    for (int i = 0; i < N; ++i)
        CHECK(handles[i].valid());

    // No two handles may alias the same slot.
    for (int i = 0; i < N; ++i)
        for (int j = i + 1; j < N; ++j)
            CHECK(handles[i] != handles[j]);

    for (int i = 0; i < N; ++i)
        destroy_pool(handles[i]);
}

static void test_concurrent_alloc_free()
{
    // N threads allocate and free from the same pool concurrently.  After all
    // threads join, live_bytes must be zero and alloc count must equal free count.
    constexpr int         N_THREADS = 4;
    constexpr int         N_ALLOCS  = 128;
    constexpr std::size_t SZ        = 32;

    PoolHandle h = create_pool("concurrent_alloc");

    std::vector<std::thread> threads;
    threads.reserve(N_THREADS);
    for (int t = 0; t < N_THREADS; ++t)
    {
        threads.emplace_back([h]{
            for (int i = 0; i < N_ALLOCS; ++i)
            {
                void* p = mem_alloc(h, SZ);
                if (p) mem_free(p);
            }
        });
    }
    for (auto& t : threads) t.join();

    PoolStats s = get_stats(h);
    CHECK(s.live_bytes   == 0);
    CHECK(s.total_allocs == s.total_frees);
    CHECK(s.total_allocs == static_cast<std::size_t>(N_THREADS * N_ALLOCS));

    destroy_pool(h);
}

static void test_concurrent_alloc_distinct_pools()
{
    // Each thread owns its own pool.  Exercises that per-bucket atomics are
    // correctly isolated: no cross-pool counter corruption should occur.
    constexpr int         N_THREADS = 4;
    constexpr int         N_ALLOCS  = 64;
    constexpr std::size_t SZ        = 16;

    PoolHandle handles[N_THREADS];
    for (int i = 0; i < N_THREADS; ++i)
        handles[i] = create_pool("distinct_pool");

    std::vector<std::thread> threads;
    threads.reserve(N_THREADS);
    for (int t = 0; t < N_THREADS; ++t)
    {
        threads.emplace_back([h = handles[t]]{
            for (int i = 0; i < N_ALLOCS; ++i)
            {
                void* p = mem_alloc(h, SZ);
                if (p) mem_free(p);
            }
        });
    }
    for (auto& t : threads) t.join();

    for (int i = 0; i < N_THREADS; ++i)
    {
        PoolStats s = get_stats(handles[i]);
        CHECK(s.live_bytes   == 0);
        CHECK(s.total_allocs == static_cast<std::size_t>(N_ALLOCS));
        CHECK(s.total_frees  == static_cast<std::size_t>(N_ALLOCS));
        destroy_pool(handles[i]);
    }
}

static void test_concurrent_stats_read()
{
    // Reader threads call get_stats / pool_count / for_each_pool while a writer
    // thread hammers alloc+free.  The read-side operations use relaxed atomic
    // loads so values may be momentarily stale, but must never cause UB or a
    // crash.  After all threads join, the stats must be exact.
    constexpr int         N_READERS = 3;
    constexpr int         N_ALLOCS  = 256;
    constexpr std::size_t SZ        = 8;

    PoolHandle h = create_pool("stats_read");
    std::atomic<bool> done { false };

    std::vector<std::thread> readers;
    readers.reserve(N_READERS);
    for (int i = 0; i < N_READERS; ++i)
    {
        readers.emplace_back([h, &done]{
            while (!done.load(std::memory_order_relaxed))
            {
                (void)get_stats(h);
                (void)pool_count();
                for_each_pool([](PoolStats, void*){}, nullptr);
            }
        });
    }

    std::thread writer([h]{
        for (int i = 0; i < N_ALLOCS; ++i)
        {
            void* p = mem_alloc(h, SZ);
            if (p) mem_free(p);
        }
    });
    writer.join();
    done.store(true, std::memory_order_relaxed);
    for (auto& r : readers) r.join();

    // All threads quiesced — stats must now be exact.
    PoolStats s = get_stats(h);
    CHECK(s.live_bytes   == 0);
    CHECK(s.total_allocs == static_cast<std::size_t>(N_ALLOCS));
    CHECK(s.total_frees  == static_cast<std::size_t>(N_ALLOCS));

    destroy_pool(h);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_create_destroy );
    RUN_TEST( test_null_pool_handle );
    RUN_TEST( test_pool_count );
    RUN_TEST( test_slot_reuse );

    RUN_TEST( test_alloc_increments_stats );
    RUN_TEST( test_multiple_allocs );
    RUN_TEST( test_free_nullptr_is_noop );
    RUN_TEST( test_alloc_zero_size );
    RUN_TEST( test_alloc_null_pool );

    RUN_TEST( test_calloc_zeroes );

    RUN_TEST( test_realloc_grow );
    RUN_TEST( test_realloc_shrink );
    RUN_TEST( test_realloc_nullptr_acts_as_alloc );
    RUN_TEST( test_realloc_zero_size_acts_as_free );
    RUN_TEST( test_realloc_pool_migration );
    RUN_TEST( test_realloc_preserves_data );

    RUN_TEST( test_for_each_pool );

    RUN_TEST( test_pool_new_delete );
    RUN_TEST( test_pool_delete_nullptr );

    RUN_TEST( test_scoped_pool );
    RUN_TEST( test_scoped_pool_handle_usable );

    RUN_TEST( test_registry_full );

    RUN_TEST( test_pool_config_system_explicit );
    RUN_TEST( test_pool_config_future_strategies );

    RUN_TEST( test_oom_handler );

    RUN_TEST( test_pool_name_truncation );
    RUN_TEST( test_pool_name_null );

    RUN_TEST( test_alloc_alignment );
    RUN_TEST( test_realloc_same_pool_counts );
    RUN_TEST( test_alloc_overflow_guard );

    RUN_TEST( test_concurrent_create_pool );
    RUN_TEST( test_concurrent_alloc_free );
    RUN_TEST( test_concurrent_alloc_distinct_pools );
    RUN_TEST( test_concurrent_stats_read );

    std::printf( "memory: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
