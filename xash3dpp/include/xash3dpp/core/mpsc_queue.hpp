#pragma once
// xash3dpp — core::MpscQueue<T, Capacity, ReserveCapacity>
//
// Library: xash3dpp_core (header-only template; no .cpp).
//
// A bounded, lock-free multi-producer / single-consumer command queue.  This
// is one half of the P-1 "queue family" (design brief:
// docs/design/thread-spawn-and-inbox-brief.md §3.1; threading-model.md §5.1).
// It is the primitive behind the future MAIN → AudioDecoder command stream and
// the MAIN-inbox service queue (P-1), but it is deliberately GENERIC — no audio
// type ever appears in `core`.  The first consumer (sound) instantiates it with
// its own POD command type in S9.7b; NetIO/worker consumers later.
//
// Algorithm (justified in detail below): a Vyukov bounded-queue.  Each physical
// slot carries a monotonically-advancing sequence number that encodes its
// generation; producers claim a ticket by CAS on a single `enqueue_pos_`
// counter, then publish their payload with a release store on the slot's
// sequence.  The single consumer reads with the matching acquire load.  On top
// of that proven handshake sits a small OCCUPANCY GATE that enforces a two-tier
// admission policy (normal region + reserved headroom) so a consumer subsystem
// policy such as sound's SND-OQ-3 STOP fast-lane composes WITHOUT forking the
// queue.
//
// Contract highlights:
//   • T must be trivially copyable (static_assert) — payloads are memcpy-safe
//     PODs; NO allocation happens in construction after the fixed slot array,
//     and NONE in try_push / push_reserved_class / try_pop.
//   • Multi-producer safe; SINGLE consumer only (try_pop is asserted to a single
//     thread in debug builds).
//   • The primitive NEVER blocks and NEVER drops silently — a push that cannot
//     be admitted returns false and the caller owns the full-queue policy.
//
// @thread-safety: multi-producer (try_push / push_reserved_class from any
// thread) + single-consumer (try_pop from one fixed thread).  Lock-free;
// wait-free-population-oblivious on the producer fast path (bounded CAS retry
// only under producer contention); no allocation; no blocking.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <xash3dpp/core/assert.hpp>

#ifndef NDEBUG
#  include <functional>  // std::hash
#  include <thread>      // std::this_thread::get_id, std::thread::id
#endif

namespace xash::core {

namespace detail {

// Smallest power of two >= n (>= 1).  constexpr so it drives the fixed array
// bound at compile time.  n == 0 yields 1.
[[nodiscard]] inline constexpr std::size_t mpsc_round_up_pow2( std::size_t n ) noexcept
{
    std::size_t p = 1;
    while( p < n )
        p <<= 1;
    return p;
}

} // namespace detail

// ---------------------------------------------------------------------------
// MpscQueue
// ---------------------------------------------------------------------------
//   T                — trivially-copyable POD message type.
//   Capacity         — normal-region logical capacity (try_push admission
//                      limit).  Must be >= 1.
//   ReserveCapacity  — reserved headroom above the normal region.  push_reserved
//                      _class may fill up to Capacity + ReserveCapacity.
//                      Default 0 (no headroom).
//
// The physical slot array is rounded up to a power of two >= Capacity +
// ReserveCapacity so slot indexing is a mask, not a modulo.  Logical admission
// is gated on EXACT occupancy, so capacity()/reserve_capacity() are honoured
// precisely regardless of the physical rounding.
template<typename T, std::size_t Capacity, std::size_t ReserveCapacity = 0>
class MpscQueue
{
    static_assert( std::is_trivially_copyable_v<T>,
        "MpscQueue<T>: T must be trivially copyable (POD message payload)." );
    static_assert( Capacity >= 1,
        "MpscQueue: Capacity must be >= 1." );

public:
    using value_type = T;

    // Logical ticket/generation counters are EXPLICITLY 64-bit, not size_t
    // (S9.7a adversarial review F1/F4): on a 32-bit target a size_t counter
    // wraps at 2^32, and at that straddle the occupancy gate's `pos >= deq`
    // guard stops holding, transiently admitting past total_capacity (the
    // seq handshake still prevents physical overwrite, but the advertised
    // logical ceiling would be violated). A 64-bit counter puts the wrap out
    // of reach on every target, so the gate's conservatism is unconditional.
    // Physical indexing stays size_t (a mask of k_phys).
    using pos_t = std::uint64_t;

    // Lock-freedom is the entire point of this type; a silent mutex fallback
    // (targets without a 64-bit CAS) would break the wait-free producer
    // contract with no other symptom.
    static_assert( std::atomic<pos_t>::is_always_lock_free,
        "MpscQueue: 64-bit atomics must be lock-free on this target." );

    MpscQueue() noexcept
    {
        // Seed each slot's sequence with its own index: slot i is "ready to be
        // filled at logical position i".  Relaxed is sufficient — no other
        // thread can observe the queue during construction.
        for( std::size_t i = 0; i < k_phys; ++i )
            slots_[i].seq.store( static_cast<pos_t>( i ), std::memory_order_relaxed );
    }

    // Exclusive-ownership primitive: atomics + slot-generation invariants make
    // copy and move meaningless.  (QJ — RAII / exclusive-ownership: delete.)
    MpscQueue( const MpscQueue & )            = delete;
    MpscQueue &operator=( const MpscQueue & ) = delete;
    MpscQueue( MpscQueue && )                 = delete;
    MpscQueue &operator=( MpscQueue && )      = delete;

    // Enqueue into the NORMAL region.  Fails (returns false) when the normal
    // region is full (occupancy >= Capacity), even if reserved headroom remains
    // — that headroom is exclusively for push_reserved_class.  Any thread.
    [[nodiscard]] bool try_push( const T &v ) noexcept
    {
        return push_impl( v, Capacity );
    }

    // Enqueue that MAY dip into the reserved headroom: admitted while occupancy
    // < Capacity + ReserveCapacity.  Lets a consumer-subsystem fast-lane policy
    // (e.g. SND-OQ-3 STOP/CHANGE) always land without a second queue.  Any
    // thread.
    [[nodiscard]] bool push_reserved_class( const T &v ) noexcept
    {
        return push_impl( v, Capacity + ReserveCapacity );
    }

    // Dequeue one message in FIFO order.  Returns false when empty (or when the
    // head producer has claimed but not yet published its slot — indistinguish
    // able from empty, and correct: the item surfaces on a later poll, in
    // order).  SINGLE CONSUMER ONLY — see assert_single_consumer().
    [[nodiscard]] bool try_pop( T &out ) noexcept
    {
        assert_single_consumer();

        // The consumer is the only writer of dequeue_pos_, so a relaxed read of
        // its own position is sufficient.
        const pos_t pos = dequeue_pos_.load( std::memory_order_relaxed );
        Slot       &s   = slots_[static_cast<std::size_t>( pos & k_mask )];

        // Acquire: pairs with the producer's release store of seq == pos + 1,
        // making the payload write visible before we read it.
        const pos_t seq = s.seq.load( std::memory_order_acquire );

        // For a single consumer at logical `pos`, the slot's sequence is either
        // `pos` (empty — waiting for a producer to publish) or `pos + 1`
        // (published, ready).  Any other value is impossible without a second
        // consumer or memory corruption.
        if( seq != pos + 1 )
        {
            XASH_ASSERT( seq == pos );  // empty; nothing else is valid here
            return false;
        }

        out = s.data;  // exclusive read: we observed the publish, producer will
                       // not touch this slot until we free it below.

        // Release: frees the slot for its next generation (logical pos + k_phys)
        // and pairs with a future producer's acquire load, guaranteeing our
        // read of `data` completes before that producer overwrites it.
        s.seq.store( pos + k_phys, std::memory_order_release );

        // Advance our position.  Relaxed: this only feeds the producers'
        // APPROXIMATE occupancy gate (policy, not correctness) — the slot-reuse
        // handshake is carried entirely by the seq release store above.
        dequeue_pos_.store( pos + 1, std::memory_order_relaxed );
        return true;
    }

    // Approximate live-message count.  RACE WINDOW: the two counters are
    // sampled separately with relaxed loads, and read-read coherence gives no
    // cross-atomic ordering — the result may be stale by a few messages and is
    // exact only when quiescent.  The TRAILING counter is sampled FIRST (S9.7a
    // review F3) so skew biases toward an over-estimate instead of an unsigned
    // underflow to a wrapped-huge value, and the result is clamped for the
    // stats/debug callers that display it.  Never used for correctness.
    [[nodiscard]] std::size_t occupancy() const noexcept
    {
        const pos_t d = dequeue_pos_.load( std::memory_order_relaxed );
        const pos_t e = enqueue_pos_.load( std::memory_order_relaxed );
        if( e <= d )
            return 0;  // consumer raced ahead of our enqueue sample
        const pos_t used = e - d;
        return static_cast<std::size_t>( used < total_capacity() ? used : total_capacity() );
    }

    [[nodiscard]] static constexpr std::size_t capacity()         noexcept { return Capacity; }
    [[nodiscard]] static constexpr std::size_t reserve_capacity() noexcept { return ReserveCapacity; }
    [[nodiscard]] static constexpr std::size_t total_capacity()   noexcept { return Capacity + ReserveCapacity; }

private:
    struct Slot
    {
        std::atomic<pos_t> seq;   // generation handshake (see algorithm)
        T                        data;
    };

    static constexpr std::size_t k_phys =
        detail::mpsc_round_up_pow2( Capacity + ReserveCapacity );
    static constexpr std::size_t k_mask = k_phys - 1;

    using sdiff_t = std::int64_t;  // signed counterpart of pos_t

    // Shared producer path.  |limit| is the admission ceiling on occupancy:
    // Capacity for try_push, Capacity + ReserveCapacity for the reserved class.
    [[nodiscard]] bool push_impl( const T &v, pos_t limit ) noexcept
    {
        // Relaxed: a ticket counter read.  A stale value only costs a CAS retry;
        // the real producer↔consumer synchronisation is the per-slot seq handshake.
        pos_t pos = enqueue_pos_.load( std::memory_order_relaxed );

        for( ;; )
        {
            // OCCUPANCY GATE (policy).  Relaxed read of dequeue_pos_: a relaxed
            // load can only return a value <= the true dequeue_pos_, so the
            // computed occupancy is an OVER-estimate — the gate is conservative
            // and never admits past |limit| (and |limit| <= k_phys, so it never
            // admits past the physical ring).  `pos` may be a stale ticket that
            // trails a fast consumer (pos < deq); in that case skip the gate and
            // let the seq check below reload a fresh ticket.  The guard needs
            // pos/deq NOT to straddle the counter's numeric wrap — guaranteed
            // by pos_t being 64-bit on every target (see its declaration);
            // physical overwrite is independently impossible via the seq
            // handshake either way.
            const pos_t deq = dequeue_pos_.load( std::memory_order_relaxed );
            if( pos >= deq && ( pos - deq ) >= limit )
                return false;

            Slot       &s   = slots_[static_cast<std::size_t>( pos & k_mask )];
            // Acquire: observe the consumer's release store that freed this slot
            // (seq == pos), ensuring the previous occupant's payload read
            // completed before we overwrite `data`.
            const pos_t seq = s.seq.load( std::memory_order_acquire );
            // Subtract in UNSIGNED (modular) arithmetic, then cast ONCE (S9.7a
            // review F2): casting each operand first and subtracting is
            // signed-overflow UB near the counter's numeric midpoint, even
            // though it yields identical bits on two's-complement hardware.
            const sdiff_t dif = static_cast<sdiff_t>( seq - pos );

            if( dif == 0 )
            {
                // Slot is free at generation `pos`.  Try to claim the ticket.
                // Relaxed on both success and failure: publication is via the
                // seq release store, not via enqueue_pos_.  On failure `pos` is
                // refreshed to the current head and we retry.
                if( enqueue_pos_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed, std::memory_order_relaxed ) )
                    break;  // claimed logical position `pos`
            }
            else if( dif < 0 )
            {
                // Physically full.  Unreachable while the occupancy gate holds
                // (limit <= k_phys), kept as a defensive backstop.
                return false;
            }
            else
            {
                // Another producer already advanced past `pos`; reload a fresh
                // ticket and retry.  Relaxed for the same reason as above.
                pos = enqueue_pos_.load( std::memory_order_relaxed );
            }
        }

        Slot &s = slots_[static_cast<std::size_t>( pos & k_mask )];
        s.data  = v;  // exclusive: we own the claimed slot until the store below

        // Release: publishes `data`; pairs with the consumer's acquire load of
        // seq, making the payload visible before the consumer reads it.
        s.seq.store( pos + 1, std::memory_order_release );
        return true;
    }

    // Debug-only single-consumer contract check: capture the first popping
    // thread and assert every later try_pop comes from the same thread.  No-op
    // in release builds.
    void assert_single_consumer() noexcept
    {
#ifndef NDEBUG
        const std::size_t tid =
            std::hash<std::thread::id>{}( std::this_thread::get_id() );
        std::size_t expected = 0;
        if( !consumer_tid_.compare_exchange_strong(
                expected, tid, std::memory_order_relaxed ) )
            XASH_ASSERT( expected == tid );  // a second consumer thread — misuse
#endif
    }

    // OPTION-SEAM (false sharing): enqueue_pos_ (producer-CAS-contended) and
    // dequeue_pos_ (consumer-owned) share adjacent storage today.  If the audio
    // command path (S9.7b+) ever profiles a false-sharing stall, separate them
    // onto distinct cache lines with `alignas`.  Deliberately deferred here so
    // the primitive stays footprint-lean and warning-clean (an unconditional
    // alignas trips MSVC C4324 across every TU that instantiates the template).
    std::atomic<pos_t> enqueue_pos_{ 0 };  // producers CAS
    std::atomic<pos_t> dequeue_pos_{ 0 };  // consumer only writes
    std::array<Slot, k_phys> slots_{};
#ifndef NDEBUG
    std::atomic<std::size_t> consumer_tid_{ 0 };  // 0 == unclaimed
#endif
};

} // namespace xash::core
