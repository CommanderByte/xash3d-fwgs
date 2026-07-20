// xash3dpp — unit tests for the P-1 queue family:
//   core::MpscQueue<T, Capacity, ReserveCapacity>
//   core::SpscRing<T, Capacity>
//
// GENERICITY PIN (design brief §3.1): core's own suite instantiates a
// NON-audio message type (TestMsg) and a non-int16 ring payload (std::uint32_t)
// — proving the primitives carry no audio vocabulary.
//
// Coverage:
//   1. MPSC multi-producer stress: N producers x M msgs, single consumer drains;
//      zero loss, zero duplication, per-producer FIFO (sequence-stamped).
//   2. No-alloc-enqueue proof: trivially-copyable static_asserts + a global
//      operator-new counter delta of zero across a full push/pop cycle.
//   3. Reserved-class semantics: normal region full -> try_push fails ->
//      push_reserved_class fills the reserve -> fails -> drain restores both.
//   4. Ring-full stall: try_write on a full ring returns 0 (partial at the
//      boundary) with unread data intact (byte-compare after drain).
//   5. Empty-ring read returns 0; occupancy tracks through wrap-around with
//      non-power-of-two chunk sizes crossing the physical wrap twice+.
//   6. SPSC threaded soak: producer jthread streams a deterministic pattern;
//      consumer verifies byte-exact sequence across many wraps (< ~2s).
//   7. Single-threaded API edge cases: capacity-1, zero-length spans,
//      exact-capacity writes.

#include <xash3dpp/core/mpsc_queue.hpp>
#include <xash3dpp/core/spsc_ring.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>   // std::malloc / std::free (operator new shim backing store)
#include <span>
#include <thread>
#include <vector>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Global operator-new counter (test-binary-local) for the no-alloc proof.
// Replacing global operator new affects the ENTIRE binary; we only sample the
// counter around the tight push/pop loop in test #2, where nothing else
// allocates, so incidental allocations elsewhere (jthread, vector, printf) are
// irrelevant.
// ---------------------------------------------------------------------------
namespace {
std::atomic<std::size_t> g_new_calls{ 0 };
} // namespace

void *operator new( std::size_t n )
{
    g_new_calls.fetch_add( 1, std::memory_order_relaxed );
    // No exceptions in this project (/EHs-c-): return malloc directly.  The
    // test never exhausts memory, so a null return is not a practical concern.
    return std::malloc( n != 0 ? n : 1 );
}
void *operator new[]( std::size_t n )
{
    g_new_calls.fetch_add( 1, std::memory_order_relaxed );
    return std::malloc( n != 0 ? n : 1 );
}
void operator delete( void *p ) noexcept              { std::free( p ); }
void operator delete( void *p, std::size_t ) noexcept { std::free( p ); }
void operator delete[]( void *p ) noexcept              { std::free( p ); }
void operator delete[]( void *p, std::size_t ) noexcept { std::free( p ); }

// ---------------------------------------------------------------------------
// Non-audio message + ring payload types (genericity pin).
// ---------------------------------------------------------------------------
struct TestMsg
{
    std::uint32_t producer;
    std::uint32_t seq;
    std::uint64_t payload;
};
static_assert( std::is_trivially_copyable_v<TestMsg> );

// A non-scalar, non-int16 ring cell to prove the ring is generic over PODs.
struct RingCell
{
    std::uint32_t a;
    std::uint32_t b;
};
static_assert( std::is_trivially_copyable_v<RingCell> );

// ===========================================================================
// 1. MPSC multi-producer stress.
//
// TEST-POWER CAVEAT (S9.7a adversarial review F6): this is a strong
// ALGORITHMIC test (zero loss, zero duplication, per-producer FIFO) but it is
// NOT a memory-order test on x86.  x86-TSO already forbids the store-store and
// load-load reorderings the release/acquire annotations guard, so downgrading
// the slot-seq handshake to relaxed would very likely still pass here.  Real
// ordering coverage needs ARM/AArch64 hardware or a model checker; the
// clang/TSan side-lane is recorded as a deferred item in the campaign ledger.
//
// The NUMERIC counter wrap (2^64 after the S9.7a widening of pos_t) is
// likewise unreachable through the public API — counters start at 0 and there
// is no seed hook — so that regime is unverified by construction rather than
// by omission (review F5).
// ===========================================================================
static void test_mpsc_stress()
{
    using Queue = xash::core::MpscQueue<TestMsg, 1024, 16>;

    constexpr int           k_producers    = 4;
    constexpr std::uint32_t k_per_producer = 25000;
    const std::uint64_t     k_expected     =
        static_cast<std::uint64_t>( k_producers ) * k_per_producer;

    auto q = std::make_unique<Queue>();

    std::vector<std::uint32_t> last_seq( k_producers, 0 );  // per-producer FIFO cursor
    std::vector<std::uint64_t> count( k_producers, 0 );

    // Producers: raw jthreads (test precedent) — spin on try_push until admitted.
    std::vector<std::jthread> producers;
    producers.reserve( k_producers );
    for( int p = 0; p < k_producers; ++p )
    {
        producers.emplace_back( [q = q.get(), p]() noexcept {
            for( std::uint32_t i = 1; i <= k_per_producer; ++i )
            {
                TestMsg m{ static_cast<std::uint32_t>( p ), i,
                           ( static_cast<std::uint64_t>( p ) << 32 ) | i };
                while( !q->try_push( m ) )
                    std::this_thread::yield();
            }
        } );
    }

    // Consumer: THIS (main) thread drains until every message is seen.
    std::uint64_t total = 0;
    while( total < k_expected )
    {
        TestMsg out{};
        if( q->try_pop( out ) )
        {
            REQUIRE( out.producer < static_cast<std::uint32_t>( k_producers ) );
            // Per-producer FIFO + zero duplication: seq must advance by exactly 1.
            CHECK_EQ( out.seq, last_seq[out.producer] + 1 );
            // Payload integrity.
            CHECK_EQ( out.payload,
                      ( static_cast<std::uint64_t>( out.producer ) << 32 ) | out.seq );
            last_seq[out.producer] = out.seq;
            ++count[out.producer];
            ++total;
        }
        else
        {
            std::this_thread::yield();
        }
    }

    // Zero loss: every producer contributed exactly k_per_producer messages.
    for( int p = 0; p < k_producers; ++p )
        CHECK_EQ( count[p], static_cast<std::uint64_t>( k_per_producer ) );

    // Queue must be empty now.
    TestMsg drain{};
    CHECK( !q->try_pop( drain ) );
    // jthreads join on destruction here.
}

// ===========================================================================
// 2. No-alloc-enqueue proof.
// ===========================================================================
static void test_no_alloc_enqueue()
{
    // Stack instance: fixed slot array, no heap involved by construction.
    xash::core::MpscQueue<TestMsg, 8, 4> q;

    // Warm the code paths once (first try_pop captures the debug consumer tid,
    // etc.) BEFORE sampling the allocation counter.
    {
        TestMsg m{ 0, 1, 42 };
        (void)q.try_push( m );
        TestMsg out{};
        (void)q.try_pop( out );
    }

    const std::size_t before = g_new_calls.load( std::memory_order_relaxed );
    for( int r = 0; r < 200; ++r )
    {
        TestMsg m{ 0, static_cast<std::uint32_t>( r ), static_cast<std::uint64_t>( r ) };
        CHECK( q.try_push( m ) );
        CHECK( q.push_reserved_class( m ) );
        TestMsg a{}, b{};
        CHECK( q.try_pop( a ) );
        CHECK( q.try_pop( b ) );
    }
    const std::size_t after = g_new_calls.load( std::memory_order_relaxed );

    // A full push/pop cycle allocated nothing.
    CHECK_EQ( after, before );
}

// ===========================================================================
// 3. Reserved-class semantics.
// ===========================================================================
static void test_reserved_class()
{
    constexpr std::size_t k_cap = 4;
    constexpr std::size_t k_res = 2;
    xash::core::MpscQueue<TestMsg, k_cap, k_res> q;

    CHECK_EQ( q.capacity(), k_cap );
    CHECK_EQ( q.reserve_capacity(), k_res );
    CHECK_EQ( q.total_capacity(), k_cap + k_res );

    TestMsg m{ 0, 0, 0 };

    // Fill the normal region.
    for( std::size_t i = 0; i < k_cap; ++i )
        CHECK( q.try_push( m ) );
    CHECK_EQ( q.occupancy(), k_cap );

    // Normal region full: try_push fails, but reserved headroom is untouched.
    CHECK( !q.try_push( m ) );

    // Reserved class fills the headroom.
    for( std::size_t i = 0; i < k_res; ++i )
        CHECK( q.push_reserved_class( m ) );
    CHECK_EQ( q.occupancy(), k_cap + k_res );

    // Now both fail — nothing left.
    CHECK( !q.push_reserved_class( m ) );
    CHECK( !q.try_push( m ) );

    // Drain fully.
    std::size_t drained = 0;
    TestMsg     out{};
    while( q.try_pop( out ) )
        ++drained;
    CHECK_EQ( drained, k_cap + k_res );
    CHECK_EQ( q.occupancy(), std::size_t{ 0 } );

    // Both admission classes restored after drain.
    CHECK( q.try_push( m ) );
    CHECK( q.push_reserved_class( m ) );
}

// ===========================================================================
// 4. Ring-full stall + graceful partial write; unread data intact.
// ===========================================================================
static void test_ring_full_stall()
{
    constexpr std::size_t k_cap = 6;
    xash::core::SpscRing<std::uint32_t, k_cap> ring;

    std::uint32_t src[k_cap];
    for( std::size_t i = 0; i < k_cap; ++i )
        src[i] = 100u + static_cast<std::uint32_t>( i );

    // Fill the ring exactly.
    CHECK_EQ( ring.try_write( std::span<const std::uint32_t>( src, k_cap ) ), k_cap );
    CHECK_EQ( ring.occupancy(), k_cap );
    CHECK_EQ( ring.space(), std::size_t{ 0 } );

    // Full ring: further write returns 0 and touches nothing.
    std::uint32_t more[3] = { 900u, 901u, 902u };
    CHECK_EQ( ring.try_write( std::span<const std::uint32_t>( more, 3 ) ), std::size_t{ 0 } );

    // Unread data must be byte-identical to what we wrote.
    std::uint32_t out[k_cap] = {};
    CHECK_EQ( ring.read( std::span<std::uint32_t>( out, k_cap ) ), k_cap );
    for( std::size_t i = 0; i < k_cap; ++i )
        CHECK_EQ( out[i], src[i] );

    // Partial-boundary stall: fill 4, then offer 5 into 2 free slots -> 2 written.
    CHECK_EQ( ring.try_write( std::span<const std::uint32_t>( src, 4 ) ), std::size_t{ 4 } );
    std::uint32_t five[5] = { 10u, 11u, 12u, 13u, 14u };
    CHECK_EQ( ring.try_write( std::span<const std::uint32_t>( five, 5 ) ), std::size_t{ 2 } );
    CHECK_EQ( ring.occupancy(), k_cap );

    // Drain and verify the intact prefix (src[0..3]) + the 2 admitted (five[0..1]).
    std::uint32_t out2[k_cap] = {};
    CHECK_EQ( ring.read( std::span<std::uint32_t>( out2, k_cap ) ), k_cap );
    for( std::size_t i = 0; i < 4; ++i )
        CHECK_EQ( out2[i], src[i] );
    CHECK_EQ( out2[4], five[0] );
    CHECK_EQ( out2[5], five[1] );
}

// ===========================================================================
// 5. Empty-ring read + occupancy across wrap-around (non-pow2 chunks).
// ===========================================================================
static void test_empty_and_wrap()
{
    constexpr std::size_t k_cap = 6;   // physical size rounds up to 8 -> wraps at 8
    xash::core::SpscRing<std::uint32_t, k_cap> ring;

    // Empty read returns 0.
    std::uint32_t sink[8] = {};
    CHECK_EQ( ring.read( std::span<std::uint32_t>( sink, 4 ) ), std::size_t{ 0 } );
    CHECK_EQ( ring.occupancy(), std::size_t{ 0 } );

    // Interleave 5-element writes with 3-element reads.  5 and 3 are not aligned
    // to the physical size (8), so the write/read cursors sweep every physical
    // slot and cross the wrap repeatedly.  next_w/next_r are the deterministic
    // value sequence; occ mirrors the ring's occupancy.
    std::uint32_t next_w = 0, next_r = 0;
    std::size_t   occ = 0;

    for( int iter = 0; iter < 20; ++iter )
    {
        std::uint32_t tmp[5];
        for( std::size_t k = 0; k < 5; ++k )
            tmp[k] = next_w + static_cast<std::uint32_t>( k );

        const std::size_t want_w = 5;
        const std::size_t exp_w  = ( want_w < ( k_cap - occ ) ) ? want_w : ( k_cap - occ );
        const std::size_t w      = ring.try_write( std::span<const std::uint32_t>( tmp, want_w ) );
        CHECK_EQ( w, exp_w );
        occ    += w;
        next_w += static_cast<std::uint32_t>( w );
        CHECK_EQ( ring.occupancy(), occ );

        std::uint32_t rd[3] = {};
        const std::size_t want_r = 3;
        const std::size_t exp_r  = ( want_r < occ ) ? want_r : occ;
        const std::size_t r      = ring.read( std::span<std::uint32_t>( rd, want_r ) );
        CHECK_EQ( r, exp_r );
        for( std::size_t k = 0; k < r; ++k )
            CHECK_EQ( rd[k], next_r + static_cast<std::uint32_t>( k ) );  // byte-exact
        occ    -= r;
        next_r += static_cast<std::uint32_t>( r );
        CHECK_EQ( ring.occupancy(), occ );
    }

    // Prove we actually swept past the physical wrap (8) at least twice.
    CHECK( next_w >= 16u );
}

// ===========================================================================
// 6. SPSC threaded soak (byte-exact across many wraps, < ~2s).
// ===========================================================================
static void test_spsc_soak()
{
    constexpr std::size_t  k_cap   = 64;
    constexpr std::uint64_t k_total = 2'000'000;  // 2M / 64 ~= 31k wraps

    xash::core::SpscRing<std::uint32_t, k_cap> ring;
    std::atomic<bool> mismatch{ false };

    // Producer streams a running counter in 7-element chunks (non-pow2).
    std::jthread producer( [&ring, &mismatch]() noexcept {
        std::uint32_t v    = 0;
        std::uint64_t sent = 0;
        while( sent < k_total )
        {
            std::uint32_t chunk[7];
            const std::size_t want =
                static_cast<std::size_t>( ( k_total - sent ) < 7 ? ( k_total - sent ) : 7 );
            for( std::size_t k = 0; k < want; ++k )
                chunk[k] = v + static_cast<std::uint32_t>( k );

            const std::size_t w = ring.try_write( std::span<const std::uint32_t>( chunk, want ) );
            v    += static_cast<std::uint32_t>( w );
            sent += w;
            if( w < want )
                std::this_thread::yield();  // ring full — graceful stall
        }
        (void)mismatch;
    } );

    // Consumer (main) verifies the byte-exact 0,1,2,... sequence in 5-el reads.
    std::uint32_t expect = 0;
    std::uint64_t got    = 0;
    while( got < k_total )
    {
        std::uint32_t rd[5];
        const std::size_t r = ring.read( std::span<std::uint32_t>( rd, 5 ) );
        for( std::size_t k = 0; k < r; ++k )
        {
            if( rd[k] != expect )
                mismatch.store( true, std::memory_order_relaxed );
            ++expect;
            ++got;
        }
        if( r == 0 )
            std::this_thread::yield();
    }

    CHECK( !mismatch.load( std::memory_order_relaxed ) );
    CHECK_EQ( got, k_total );
    // producer jthread joins on destruction.
}

// ===========================================================================
// 7. Single-threaded API edge cases.
// ===========================================================================
static void test_edge_cases()
{
    // --- Capacity-1 MPSC queue. ---
    xash::core::MpscQueue<TestMsg, 1, 0> q1;
    TestMsg m{ 0, 1, 7 };
    TestMsg out{};
    CHECK( q1.try_push( m ) );
    CHECK( !q1.try_push( m ) );   // full at 1
    CHECK( q1.try_pop( out ) );
    CHECK_EQ( out.payload, std::uint64_t{ 7 } );
    CHECK( !q1.try_pop( out ) );  // empty

    // --- Capacity-1 ring: zero-length spans + exact-capacity write. ---
    xash::core::SpscRing<std::uint32_t, 1> r1;
    CHECK_EQ( r1.try_write( std::span<const std::uint32_t>{} ), std::size_t{ 0 } );  // zero-length
    CHECK_EQ( r1.read( std::span<std::uint32_t>{} ), std::size_t{ 0 } );             // zero-length
    std::uint32_t one = 7;
    CHECK_EQ( r1.try_write( std::span<const std::uint32_t>( &one, 1 ) ), std::size_t{ 1 } );  // exact
    CHECK_EQ( r1.try_write( std::span<const std::uint32_t>( &one, 1 ) ), std::size_t{ 0 } );  // full
    std::uint32_t o = 0;
    CHECK_EQ( r1.read( std::span<std::uint32_t>( &o, 1 ) ), std::size_t{ 1 } );
    CHECK_EQ( o, 7u );

    // --- Exact-capacity write on a larger ring, with a non-scalar POD payload. ---
    xash::core::SpscRing<RingCell, 5> r5;
    RingCell cells[5];
    for( std::size_t i = 0; i < 5; ++i )
        cells[i] = RingCell{ static_cast<std::uint32_t>( i ), static_cast<std::uint32_t>( 100 + i ) };
    CHECK_EQ( r5.try_write( std::span<const RingCell>( cells, 5 ) ), std::size_t{ 5 } );  // exact cap
    CHECK_EQ( r5.try_write( std::span<const RingCell>( cells, 5 ) ), std::size_t{ 0 } );  // full

    // Zero-length write on a full ring stays 0 and leaves occupancy untouched.
    CHECK_EQ( r5.try_write( std::span<const RingCell>{} ), std::size_t{ 0 } );
    CHECK_EQ( r5.occupancy(), std::size_t{ 5 } );

    RingCell back[5];
    CHECK_EQ( r5.read( std::span<RingCell>( back, 5 ) ), std::size_t{ 5 } );
    for( std::size_t i = 0; i < 5; ++i )
    {
        CHECK_EQ( back[i].a, static_cast<std::uint32_t>( i ) );
        CHECK_EQ( back[i].b, static_cast<std::uint32_t>( 100 + i ) );
    }
    CHECK( r5.empty() );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_mpsc_stress );
    RUN_TEST( test_no_alloc_enqueue );
    RUN_TEST( test_reserved_class );
    RUN_TEST( test_ring_full_stall );
    RUN_TEST( test_empty_and_wrap );
    RUN_TEST( test_spsc_soak );
    RUN_TEST( test_edge_cases );

    std::printf( "queue_family: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
