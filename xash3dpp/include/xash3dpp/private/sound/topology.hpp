#pragma once
// xash3dpp — the sound thread topology (Chunk 9, slice S9.7b).
// Legacy reference: NONE — legacy is single-threaded.  The closest analogue is
// the SDL2 backend's `SNDDMA_BeginPainting`/`SNDDMA_Submit` lock bracket
// (s_sdl2.c:184-200), which is exactly what the lock-free SPSC ring below
// replaces (sound-boundary.md §SNDDMA platform seam, SND-OQ-5).
//
// Boundary spec: docs/boundaries/sound-boundary.md §Threading (ratified
// topology), Open questions SND-OQ-2 (quiesce/epoch/shutdown order),
// SND-OQ-3 (queue-full policy), SND-OQ-5 (int16 ring payload).
// Design: threading-model.md §3.4 (Model B), §5.1/§5.2, §11;
//         thread-spawn-and-inbox-brief.md §3.2/§3.3.
//
// ===========================================================================
//   T_Main ──MPSC (AudioCommandQueue)──► T_AudioDecoder ──SPSC (PcmRing)──►
//   T_AudioCallback
// ---------------------------------------------------------------------------
// T_AudioDecoder  : owns the channel array, VOX bindings, DSP delay lines, the
//                   mix clock and the paint.  Applies commands, paints one
//                   block per step, writes int16 interleaved-stereo frames into
//                   the ring (graceful stall when full).
// T_AudioCallback : RingFillSource::fill() — reads the ring, zero-fills the
//                   shortfall and bumps the always-on `underruns` counter.
//                   Wait-free: no lock, no allocation, no logging.
// ===========================================================================
//
// @thread-safety: every member documents its owning role.  start()/stop()/
// submit()/flush()/channel_snapshot()/drain_mouth() are T_Main;
// decoder_step() is T_AudioDecoder (asserted); fill() is T_AudioCallback
// (asserted).

#include <xash3dpp/core/spsc_ring.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/platform/thread.hpp>
#include <xash3dpp/private/sound/audio_command.hpp>
#include <xash3dpp/sound/device.hpp>
#include <xash3dpp/sound/providers.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace xash::sound {

// ---------------------------------------------------------------------------
// PcmRing (SND-OQ-5) — interleaved-stereo int16 device-format frames, byte-
// identical to the legacy DMA ring.  NO float stage: the mix kernels already
// produce clamped int32 accumulator values and S_TransferPaintBuffer's CLIP16
// narrow is the last arithmetic step, so the ring is a straight memcpy of
// exactly what legacy would have written into `snd.buffer`.
// ---------------------------------------------------------------------------
using PcmRing = ::xash::core::SpscRing<std::int16_t, ::xash::limits::sound_pcm_ring_samples>;

// ---------------------------------------------------------------------------
// RingFillSource — the T_AudioCallback consumer (Q-23's pull inversion).
//
// @thread-safety: fill() runs on T_AudioCallback (real-time).  Wait-free by
// construction: one acquire load + one release store inside SpscRing::read(),
// a loop of stores for the silence tail, and one relaxed fetch_add.  No lock,
// no allocation, no logging (threading-model §Forbidden audio-callback
// patterns).
// ---------------------------------------------------------------------------
class RingFillSource final : public IAudioFillSource
{
public:
    explicit RingFillSource( SoundStats &stats ) noexcept : stats_( &stats ) {}

    void fill( std::span<std::int16_t> out ) noexcept override;

    // The ring is created by start() and destroyed by stop() (see the note on
    // AudioTopology::ring_), so the binding is a single atomic slot rather than
    // a fixed reference.  A null ring is simply an underrun — which is the
    // correct behaviour for the window between detach and teardown.
    void set_ring( PcmRing *ring ) noexcept { ring_.store( ring, std::memory_order_release ); }

    // Number of fill() invocations currently INSIDE the function body
    // (concurrency CONC-5).  Detaching the fill source from the device stops
    // NEW entries, but with internal_pump == false — the shape a real SDL/OS
    // backend uses — a callback already inside fill() cannot be joined, so
    // AudioTopology::stop() spins on this reaching zero before destroying the
    // ring.  POST-CONDITION of that spin: no thread is executing fill(), and
    // the fill source has already been detached, so `*ring_` is unreachable and
    // the ring may be destroyed.
    //
    // Wait-free by construction: one relaxed-family fetch_add on entry and one
    // release fetch_sub on exit.  No lock, no allocation, no blocking — the
    // callback never waits for stop(), only the other way round.
    [[nodiscard]] int in_callback() const noexcept { return in_callback_.load( std::memory_order_acquire ); }

private:
    std::atomic<PcmRing *> ring_ { nullptr }; // @lifetime: owned by AudioTopology
    SoundStats            *stats_;            // @lifetime: owned by Sound::Impl (outlives this)
    std::atomic<int>       in_callback_ { 0 };// see in_callback() — CONC-5 quiesce counter
};

// ---------------------------------------------------------------------------
// MouthSlots (SND-OQ-1 reverse channel) — the ONLY T_AudioDecoder -> T_Main
// data flow.  The mix computes `mouthopen` for voice/stream channels and calls
// set_mouth_open(); that write lands in a RELAXED atomic slot (never a direct
// cl_entity_t.mouth poke off T_Main).  T_Main drains the dirty slots into the
// injected IMouthSink once per frame.
//
// NOTE: no producer exists yet — the mixer's two mouth write sites are still
// deferred (mixer.cpp's "(S9.4) mouth ... omitted").  The SLOT is built here
// because SND-OQ-1's ratified shape names it, so whichever slice wires the
// mouth amplitude has no cross-thread decision left to make.
//
// PAIR ATOMICITY (concurrency CONC-4).  (entnum, mouthopen) is published as ONE
// std::atomic<std::uint64_t> — entnum in the high 32 bits, the mouthopen bit
// pattern in the low 32 — so the pair can never TEAR.  The earlier shape (two
// independent atomics plus a publish ticket) was a seqlock with no validation
// read on the consumer side: a drain could pair one entity's number with a
// different entity's amplitude after a hash collision, and silently animate the
// wrong mouth.  Packing removes the failure mode rather than papering over it
// with a retry loop, and it is what SND-OQ-1's ratified "relaxed atomic slot"
// shape describes.  Relaxed ordering is sufficient and intended: the payload is
// self-contained (no other data is published alongside it), so there is nothing
// for an acquire/release pair to order.
//
// @thread-safety: set_mouth_open() from T_AudioDecoder (asserted); drain() from
// T_Main (asserted).  `seen_` is T_Main-private state and is never touched by
// the producer.
// ---------------------------------------------------------------------------
class MouthSlots final : public IMouthSink
{
public:
    // T_AudioDecoder (asserted).  entnum hashes into the fixed table; a
    // collision costs a stale amplitude for one entity for one frame, never a
    // race and (since CONC-4) never a mismatched pair either.
    void set_mouth_open( int entnum, int mouthopen ) noexcept override;

    // T_Main (asserted): forward every slot whose published pair changed since
    // the last drain to `out` (null -> the updates are consumed and discarded).
    void drain( IMouthSink *out ) noexcept;

private:
    // Pack/unpack the (entnum, mouthopen) pair.  int -> uint32 conversions are
    // bit-pattern preserving on every target (two's complement, C++20 §6.8.2).
    [[nodiscard]] static constexpr std::uint64_t pack( int entnum, int mouthopen ) noexcept
    {
        return ( static_cast<std::uint64_t>( static_cast<std::uint32_t>( entnum ) ) << 32 ) |
               static_cast<std::uint64_t>( static_cast<std::uint32_t>( mouthopen ) );
    }
    [[nodiscard]] static constexpr int packed_entnum( std::uint64_t v ) noexcept
    {
        return static_cast<int>( static_cast<std::uint32_t>( v >> 32 ) );
    }
    [[nodiscard]] static constexpr int packed_mouthopen( std::uint64_t v ) noexcept
    {
        return static_cast<int>( static_cast<std::uint32_t>( v & 0xFFFFFFFFu ) );
    }

    // pack(0, 0) == 0, which is also the initial value of every slot AND of
    // every `seen_` entry — so a fresh table drains as "nothing changed", which
    // is exactly right.
    std::array<std::atomic<std::uint64_t>, ::xash::limits::sound_mouth_slots> slots_ {};
    std::array<std::uint64_t, ::xash::limits::sound_mouth_slots>              seen_ {}; // T_Main-private
};

// ---------------------------------------------------------------------------
// PublishedChannel — the decoder's copy of one occupied channel, published for
// the P-4 introspection surface (Sound::channels_snapshot()).  T_Main cannot
// read Mixer::channels() while the decoder owns it, so the decoder snapshots on
// request instead.  The registry-derived sfx NAME is deliberately NOT resolved
// here: the registry is T_Main-owned, so T_Main resolves it after reading this.
// ---------------------------------------------------------------------------
struct PublishedChannel
{
    std::size_t          index        = 0;
    ::xash::abi::sound_t sfx_handle   = k_invalid_sound_handle;
    int                  entnum       = 0;
    Vec3                 origin       {};
    int                  leftvol      = 0;
    int                  rightvol     = 0;
    double               sample       = 0.0;
    bool                 is_sentence  = false;
    std::string          sentence_name;
};

// ---------------------------------------------------------------------------
// TopologyParams — everything AudioTopology borrows.  All pointers must
// outlive the topology.
// ---------------------------------------------------------------------------
struct TopologyParams
{
    ChannelApplyContext *ctx    = nullptr; // the decoder-owned channel state
    SoundStats          *stats  = nullptr;
    IAudioDevice        *device = nullptr; // fill-source registration target (nullable)

    // Spawn the internal T_AudioCallback pump.  True for every device that does
    // NOT drive its own callback thread — which today is every device in the
    // tree (NullDevice never invokes the source; SinkDevice is synchronous).
    // A real SDL backend sets this false and drives fill() from the OS callback
    // thread (which must register ThreadRole::AudioCallback itself).
    bool internal_pump = true;

    // Spawn the T_AudioDecoder thread.  False lets a caller drive
    // decoder_step() itself from a thread it registered as AudioDecoder — the
    // deterministic shape the topology tests use.  Production always true.
    bool internal_decoder = true;
};

// ---------------------------------------------------------------------------
// AudioTopology
// ---------------------------------------------------------------------------
class AudioTopology
{
public:
    explicit AudioTopology( const TopologyParams &params ) noexcept;
    ~AudioTopology();

    AudioTopology( const AudioTopology & )            = delete;
    AudioTopology &operator=( const AudioTopology & ) = delete;

    // --- lifecycle (T_Main) -------------------------------------------------

    // Spawn T_AudioDecoder + (optionally) the T_AudioCallback pump and register
    // the ring fill source on the device.  Idempotent-safe: a second start()
    // while running is a no-op returning true.
    [[nodiscard]] bool start() noexcept;

    // SND-OQ-2 shutdown order, in this exact sequence:
    //   1. stop accepting commands (AudioCommandQueue::set_accepting(false)) —
    //      also releases any producer parked in the bounded block;
    //   2. detach the fill source from the device and deactivate it, so an
    //      EXTERNAL (OS) callback can no longer enter fill();
    //   3. join the device pump  (T_AudioCallback is gone -> no ring reader);
    //   4. join the decoder      (T_AudioDecoder is gone -> no ring writer, no
    //      channel state mutation, and every borrowed AudioData reference is
    //      released by the final free_all_channels());
    //   4b. spin until RingFillSource::in_callback() reaches zero — an EXTERNAL
    //      (OS) callback already inside fill() cannot be joined, and step 2 only
    //      stops NEW entries (CONC-5).  A no-op when the internal pump is used;
    //      step 3's join already implies it.
    //   5. tear down the ring/scratch (safe: no reader can be inside fill(),
    //      and the writer is joined).
    // Idempotent.
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept { return running_.load( std::memory_order_acquire ); }

    // --- command stream (T_Main) -------------------------------------------

    // SND-OQ-3 admission.  `out_dropped` is set only for a GENUINE drop (see
    // AudioCommandQueue::submit); the caller bumps SoundStats::dropped_sounds.
    [[nodiscard]] bool submit( const AudioCommand &cmd, bool *out_dropped = nullptr ) noexcept;

    // SND-OQ-2 quiesce fence.  Enqueues a FlushEpoch in the RESERVED class (so
    // it can never be refused by a START-saturated normal region) and blocks —
    // bounded by limits::sound_flush_spin_max — until the decoder acks it.
    //
    // CONTRACT: after a stop command for sfx X (AlterChannel(SND_STOP) or
    // StopAllSounds) followed by a flush() that RETURNED TRUE, the decoder can
    // never again touch X's AudioData, so T_Main may free it.  Three facts make
    // that airtight, by construction rather than by timing:
    //   (a) the MPSC is FIFO, so popping the FlushEpoch proves the stop was
    //       already applied and X's MixChannel::source is null;
    //   (b) an in-flight command cannot still carry a borrowed pointer for the
    //       same FIFO reason;
    //   (c) EVERY decoder-side dereference of borrowed audio is sequenced
    //       BEFORE the FlushEpoch pop on the same thread.  (The earlier wording
    //       — "the decoder dereferences MixChannel::source only inside
    //       paint_channels()" — was simply false, CONC-10: the drain itself
    //       dereferences borrowed audio, e.g. apply_start()'s
    //       `target.source->flags` audibility test and VOX word resolution.
    //       What actually holds is the sequencing: the decoder is single
    //       threaded, the pops before the fence and the paint of the PREVIOUS
    //       step both happen-before this pop, and the paint AFTER it can only
    //       see the post-stop channel state.)
    // The ack is a RELEASE store paired with T_Main's ACQUIRE load, so all of
    // the above is visible to main before flush() returns true.
    //
    // The fence itself is non-destructive: unrelated channels keep playing.
    //
    // RETURN VALUE (concurrency CONC-3) — flush() must NOT fail open, because
    // its entire purpose is to be the licence to free borrowed audio:
    //   true  — the ack was observed, or the topology is not running (no
    //           decoder -> no borrower -> nothing to fence).
    //   false — the FlushEpoch was REFUSED (a saturated reserve region, or the
    //           queue closed under a concurrent stop), or the spin bound
    //           expired with no ack.  The caller must NOT treat prior audio as
    //           quiesced: a borrowed AudioData may still be reachable from a
    //           live channel or an in-flight command.
    [[nodiscard]] bool flush() noexcept;

    [[nodiscard]] std::uint64_t acked_epoch() const noexcept
    {
        return acked_epoch_.load( std::memory_order_acquire );
    }

    // --- decoder (T_AudioDecoder) ------------------------------------------

    // One iteration of the decoder loop: drain the command stream, paint one
    // block, write it to the ring.  Asserts ThreadRole::AudioDecoder.  Exposed
    // (rather than being buried in the thread body) so tests can drive the
    // decoder deterministically from a thread that registered the role.
    // Returns true if it did any work (used by the loop to decide whether to
    // park).
    [[nodiscard]] bool decoder_step() noexcept;

    // --- introspection / reverse channel (T_Main) --------------------------

    // Ask the decoder for a channel snapshot and wait (bounded) for it.  When
    // the topology is not running this returns false and the caller reads the
    // live array instead.  COLD path (Q-13): `out` is caller-owned and sized on
    // demand; no pre-reserve budget applies.
    [[nodiscard]] bool channel_snapshot( std::vector<PublishedChannel> &out ) noexcept;

    [[nodiscard]] int total_channels() const noexcept
    {
        return total_channels_.load( std::memory_order_relaxed );
    }

    [[nodiscard]] IAudioFillSource  &fill_source() noexcept { return fill_; }
    [[nodiscard]] MouthSlots        &mouth_slots() noexcept { return mouth_; }
    [[nodiscard]] AudioCommandQueue *queue() noexcept { return queue_.get(); }
    [[nodiscard]] PcmRing           *ring() noexcept { return ring_.get(); }

private:
    static void decoder_thread_main( void *user ) noexcept;
    static void pump_thread_main( void *user ) noexcept;

    void publish_channels_if_requested() noexcept; // T_AudioDecoder

    ChannelApplyContext *ctx_;
    SoundStats          *stats_;
    IAudioDevice        *device_;
    bool                 internal_pump_;
    bool                 internal_decoder_;

    // The MPSC and the SPSC ring both LATCH their single-consumer (and, for the
    // ring, single-producer) thread identity on first use, in debug builds, to
    // enforce their contracts.  A stop()/start() cycle spawns FRESH decoder and
    // callback threads, so both primitives are constructed by start() and
    // destroyed by stop() — which is exactly the "tear down ring/scratch" step
    // the SND-OQ-2 shutdown order calls for anyway.
    //
    // compliance-allow(unique-ptr-nonpimpl, make-unique-outside-pimpl):
    // AudioCommandQueue and PcmRing are
    // built with std::make_unique outside a pimpl Impl, against Q-22 rule 4
    // (conformance F3), and that is deliberate.  Both are FIXED-STORAGE
    // lock-free primitives whose contract is precisely that they never touch an
    // allocator after construction — a pool buys nothing (there is no
    // steady-state allocation to pool) and its rules actively do not fit: the
    // pool has no over-aligned-storage guarantee for the cache-line-padded
    // atomics inside MpscQueue/SpscRing, and its lifetime is Sound's whole
    // session whereas these are PER-RUN (recreated by every start(), destroyed
    // by every stop(), so a fresh decoder/callback thread pair can latch its own
    // single-consumer identity).  Heap-once-per-run via unique_ptr is the
    // honest shape; they are not pimpls and never will be.
    std::unique_ptr<AudioCommandQueue> queue_;
    // compliance-allow(unique-ptr-nonpimpl, make-unique-outside-pimpl): same
    // rationale as queue_ directly above — fixed-storage lock-free primitive,
    // per-run lifetime, never allocates after construction, not a pimpl.
    std::unique_ptr<PcmRing> ring_;

    RingFillSource fill_;
    MouthSlots     mouth_ {};

    std::atomic<bool> running_         { false }; // topology as a whole (T_Main writes)
    std::atomic<bool> decoder_running_ { false };
    std::atomic<bool> pump_running_    { false };

    // SND-OQ-2 epoch/ack pair.  submitted_ is T_Main-only; acked_ is written
    // ONLY by the decoder (release) and read by T_Main (acquire).
    std::uint64_t              submitted_epoch_ = 0;
    std::atomic<std::uint64_t> acked_epoch_ { 0 };

    std::atomic<int> total_channels_ { 0 }; // decoder publishes, T_Main reads (relaxed; cosmetic)

    // P-4 snapshot handshake: T_Main raises `request`, the decoder fills
    // `published_` under `publish_mutex_` and bumps `publish_seq_`.
    std::atomic<bool>          snapshot_request_ { false };
    std::atomic<std::uint64_t> publish_seq_ { 0 };
    std::mutex                 publish_mutex_;
    // COLD path (Q-13): filled only when T_Main explicitly asks for a snapshot
    // (the s_show/P-4 introspection surface), never per mix block — no
    // @pre-reserved budget applies.
    std::vector<PublishedChannel> published_; // guarded by publish_mutex_

    // T_AudioCallback pump scratch.
    // @pre-reserved: sound_device_pump_frames * 2 (assigned in start(); the
    //   callback path must never allocate).
    std::vector<std::int16_t> pump_scratch_;

    ::xash::platform::JoinHandle decoder_thread_;
    ::xash::platform::JoinHandle pump_thread_;
};

} // namespace xash::sound
