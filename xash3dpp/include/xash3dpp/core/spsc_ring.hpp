#pragma once
// xash3dpp — core::SpscRing<T, Capacity>
//
// Library: xash3dpp_core (header-only template; no .cpp).
//
// A fixed-size, lock-free single-producer / single-consumer ring buffer — the
// second half of the P-1 "queue family" (design brief:
// docs/design/thread-spawn-and-inbox-brief.md §3.2; threading-model.md §5.2).
// Sound's PCM ring (AudioDecoder → AudioCallback) is the first instantiation
// (SND-OQ-5), NetIO-class byte streams later, but the type is GENERIC — no
// audio type (no int16 payload, no frame vocabulary) appears in `core`.
//
// This type RETIRES the legacy `s_rawend` volatile-peek idiom: occupancy is a
// first-class accessor (the G-3 read pattern), never a bare `volatile` read of
// a shared write cursor.
//
// Synchronisation (threading-model.md §5.2 — "no atomic CAS is required; SPSC
// with sequential producer/consumer is safe with a single release store and
// acquire load on the head/tail indices"):
//   • Producer owns head_ (the write cursor); consumer owns tail_ (the read
//     cursor).  Both are monotonically-advancing 64-bit counters (explicitly
//     uint64_t, NOT size_t — see pos_t); the physical slot for a counter is
//     `counter & mask`.
//   • try_write does exactly ONE acquire load (of tail_, the other side's
//     cursor) and ONE release store (of head_, publishing the written data).
//   • read does exactly ONE acquire load (of head_) and ONE release store (of
//     tail_, freeing the consumed data).  No other fences.
//   (Counter wrap is CLEAN regardless of width: occupancy is a modular
//    unsigned difference bounded by Capacity, and indexing is a power-of-two
//    mask.  The width still matters for the accessors and for reasoning
//    parity with MpscQueue, so pos_t is 64-bit on every target — a size_t
//    counter would wrap after ~13-27 h of continuous PCM on a 32-bit build
//    (S9.7a adversarial review F4).)
//
// Contract highlights:
//   • T must be trivially copyable (static_assert) — bulk memcpy-style transfer.
//   • Fixed storage; NO allocation after construction, none in try_write / read.
//   • Producer stalls GRACEFULLY: try_write writes only what fits and returns
//     that count; it NEVER overwrites unread data.
//   • Consumer owns the silence path: read returns 0 on empty; the caller
//     decides what an empty ring means (e.g. output silence + bump an underrun
//     counter — that framing policy lives in the consuming subsystem, not here).
//
// @thread-safety: single-producer (try_write from one fixed thread) +
// single-consumer (read from one fixed thread).  Lock-free; wait-free; no
// allocation; no blocking.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <xash3dpp/core/assert.hpp>

#ifndef NDEBUG
#  include <functional>  // std::hash
#  include <thread>      // std::this_thread::get_id, std::thread::id
#endif

namespace xash::core {

namespace detail {

// Smallest power of two >= n (>= 1).  n == 0 yields 1.
[[nodiscard]] inline constexpr std::size_t spsc_round_up_pow2( std::size_t n ) noexcept
{
    std::size_t p = 1;
    while( p < n )
        p <<= 1;
    return p;
}

} // namespace detail

// ---------------------------------------------------------------------------
// SpscRing
// ---------------------------------------------------------------------------
//   T         — trivially-copyable element type (e.g. a mixed PCM sample; kept
//               generic here — the sound slice supplies the concrete type).
//   Capacity  — logical element capacity.  Must be >= 1.  Occupancy is honoured
//               exactly; the physical array is rounded up to a power of two >=
//               Capacity so wrapping is a mask.
template<typename T, std::size_t Capacity>
class SpscRing
{
    static_assert( std::is_trivially_copyable_v<T>,
        "SpscRing<T>: T must be trivially copyable." );
    static_assert( Capacity >= 1,
        "SpscRing: Capacity must be >= 1." );

public:
    using value_type = T;

    // Explicitly 64-bit cursors (S9.7a review F4): see the header comment.
    using pos_t = std::uint64_t;

    static_assert( std::atomic<pos_t>::is_always_lock_free,
        "SpscRing: 64-bit atomics must be lock-free on this target." );

    SpscRing() noexcept = default;

    // Exclusive-ownership primitive (atomics + cursor invariants): no copy/move.
    SpscRing( const SpscRing & )            = delete;
    SpscRing &operator=( const SpscRing & ) = delete;
    SpscRing( SpscRing && )                 = delete;
    SpscRing &operator=( SpscRing && )      = delete;

    // Write as many of |src|'s elements as currently fit; returns the count
    // actually written (0 .. src.size()).  A short return is the graceful-stall
    // signal — the producer retries the remainder later.  Never overwrites
    // unread data.  SINGLE PRODUCER ONLY.
    [[nodiscard]] std::size_t try_write( std::span<const T> src ) noexcept
    {
        assert_single_producer();

        // Relaxed: producer owns head_, reading its own cursor needs no ordering.
        const pos_t head = head_.load( std::memory_order_relaxed );
        // Acquire: pairs with the consumer's release store of tail_, so freed
        // slots are visible here before we reuse them (no overwrite of data the
        // consumer has not finished reading).
        const pos_t tail = tail_.load( std::memory_order_acquire );

        const std::size_t used = static_cast<std::size_t>( head - tail ); // 0 .. Capacity
        const std::size_t free = Capacity - used;
        const std::size_t n    = ( src.size() < free ) ? src.size() : free;
        if( n == 0 )
            return 0;

        // Copy into [head, head+n) with a single wrap split.  n <= free <=
        // Capacity <= k_phys guarantees the two segments never touch the unread
        // region [tail, head).
        const std::size_t idx   = static_cast<std::size_t>( head & k_mask );
        const std::size_t first = ( n < ( k_phys - idx ) ) ? n : ( k_phys - idx );
        std::copy_n( src.data(), first, &buffer_[idx] );
        if( n > first )
            std::copy_n( src.data() + first, n - first, &buffer_[0] );

        // Release: publishes the written elements; pairs with the consumer's
        // acquire load of head_.
        head_.store( head + n, std::memory_order_release );
        return n;
    }

    // Read up to |dst|.size() available elements into |dst|; returns the count
    // read (0 when empty).  SINGLE CONSUMER ONLY.
    [[nodiscard]] std::size_t read( std::span<T> dst ) noexcept
    {
        assert_single_consumer();

        // Relaxed: consumer owns tail_.
        const pos_t tail = tail_.load( std::memory_order_relaxed );
        // Acquire: pairs with the producer's release store of head_, so written
        // payload is visible before we read it.
        const pos_t head = head_.load( std::memory_order_acquire );

        const std::size_t avail = static_cast<std::size_t>( head - tail ); // 0 .. Capacity
        const std::size_t n     = ( dst.size() < avail ) ? dst.size() : avail;
        if( n == 0 )
            return 0;

        const std::size_t idx   = static_cast<std::size_t>( tail & k_mask );
        const std::size_t first = ( n < ( k_phys - idx ) ) ? n : ( k_phys - idx );
        std::copy_n( &buffer_[idx], first, dst.data() );
        if( n > first )
            std::copy_n( &buffer_[0], n - first, dst.data() + first );

        // Release: frees the consumed elements; pairs with the producer's
        // acquire load of tail_.
        tail_.store( tail + n, std::memory_order_release );
        return n;
    }

    // Approximate element count available to read.  RACE WINDOW: head_ and
    // tail_ are sampled separately with relaxed loads and read-read coherence
    // orders nothing between them, so the value may be stale by a few elements
    // and is exact only when quiescent.  The TRAILING cursor is sampled FIRST
    // (S9.7a review F3) so skew biases toward an over-estimate rather than an
    // unsigned underflow to a wrapped-huge value, and the result is clamped.
    // This is the G-3 read pattern that replaces the legacy volatile `s_rawend`
    // peek.
    [[nodiscard]] std::size_t occupancy() const noexcept
    {
        const pos_t t = tail_.load( std::memory_order_relaxed );
        const pos_t h = head_.load( std::memory_order_relaxed );
        if( h <= t )
            return 0;  // consumer raced ahead of our head sample
        const pos_t used = h - t;
        return static_cast<std::size_t>( used < Capacity ? used : Capacity );
    }

    // Approximate free space (Capacity - occupancy); same race window.
    [[nodiscard]] std::size_t space() const noexcept
    {
        return Capacity - occupancy();
    }

    [[nodiscard]] bool empty() const noexcept { return occupancy() == 0; }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    static constexpr std::size_t k_phys = detail::spsc_round_up_pow2( Capacity );
    static constexpr std::size_t k_mask = k_phys - 1;

    void assert_single_producer() noexcept
    {
#ifndef NDEBUG
        const std::size_t tid =
            std::hash<std::thread::id>{}( std::this_thread::get_id() );
        std::size_t expected = 0;
        if( !producer_tid_.compare_exchange_strong(
                expected, tid, std::memory_order_relaxed ) )
            XASH_ASSERT( expected == tid );  // a second producer thread — misuse
#endif
    }

    void assert_single_consumer() noexcept
    {
#ifndef NDEBUG
        const std::size_t tid =
            std::hash<std::thread::id>{}( std::this_thread::get_id() );
        std::size_t expected = 0;
        if( !consumer_tid_.compare_exchange_strong(
                expected, tid, std::memory_order_relaxed ) )
            XASH_ASSERT( expected == tid );
#endif
    }

    // OPTION-SEAM (false sharing): head_ (producer) and tail_ (consumer) share
    // adjacent storage today.  The PCM hot path (S9.7b+) may want them on
    // distinct cache lines (`alignas`) to avoid producer/consumer false sharing;
    // deferred until profiled so the primitive stays footprint-lean and
    // warning-clean (an unconditional alignas trips MSVC C4324 in every
    // instantiating TU).
    std::atomic<pos_t> head_{ 0 };  // producer write cursor
    std::atomic<pos_t> tail_{ 0 };  // consumer read cursor
    std::array<T, k_phys>    buffer_{};
#ifndef NDEBUG
    std::atomic<std::size_t> producer_tid_{ 0 };  // 0 == unclaimed
    std::atomic<std::size_t> consumer_tid_{ 0 };
#endif
};

} // namespace xash::core
