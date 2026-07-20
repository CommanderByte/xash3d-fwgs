#pragma once
// xash3dpp — audio output device seam (Chunk 9, slice S9.1).
// Legacy reference: engine/platform/{platform.h:390-402 (the SNDDMA_* seam),
// sdl2/s_sdl2.c (SDL_OpenAudioDevice + the SDL audio callback), stub/s_stub.c
// (the null backend — SNDDMA_Init returns false)}.
// Boundary spec: docs/boundaries/sound-boundary.md §SNDDMA platform seam,
// §Threading (T_AudioCallback), Open questions SND-OQ-5 (int16 ring payload).
//
// The seam is PULL-shaped (Q-23): the device drives an audio callback that
// PULLS mixed frames from a registered IAudioFillSource, inverting the legacy
// PUSH model (the mixer wrote snd.buffer, SNDDMA_Submit copied it out under the
// BeginPainting/Submit lock).  The lock bracket is what the lock-free SPSC ring
// replaces (SND-OQ-5); the ring itself is not built in this slice.
//
// ===========================================================================
// SNDDMA crosswalk (platform.h:390-402  ->  IAudioDevice / IAudioFillSource)
// ---------------------------------------------------------------------------
//   SNDDMA_Init          ->  IAudioDevice::open(const DeviceSpec&) -> DeviceCaps
//   SNDDMA_Shutdown      ->  IAudioDevice::close()
//   SNDDMA_Activate      ->  IAudioDevice::set_active(bool)
//   SNDDMA_BeginPainting  \   the mixer/consumer lock bracket the SPSC ring
//   SNDDMA_Submit         /   replaces (NOT a device method; see SND-OQ-5)
//   (no legacy analogue)  ->  IAudioDevice::set_fill_source(IAudioFillSource*)
//                             the pull inversion: the device requests frames
//                             from the source instead of the mixer pushing a
//                             filled buffer.  The SDL audio callback that read
//                             snd.buffer becomes IAudioFillSource::fill().
// ===========================================================================
//
// @thread-safety: open/close/set_active/set_fill_source are lifecycle calls made
// on T_Main.  IAudioFillSource::fill() runs on T_AudioCallback (real-time): it
// must not allocate, lock, or block (threading-model §Forbidden audio-callback
// patterns).  Ring payload is interleaved-stereo int16 (SND-OQ-5) so the
// transfer stays a straight memcpy with no extra clamp/convert stage.

#include <xash3dpp/limits.hpp>       // sound_dma_speed
#include <xash3dpp/sound/errors.hpp> // Result<T>, SoundError

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace xash::sound {

// ---------------------------------------------------------------------------
// DeviceSpec / DeviceCaps — the requested vs. negotiated output format.  The
// GoldSrc DMA ring is always 44.1 kHz / 2-channel / 16-bit (sound.h:27-31), so
// those are the defaults; a backend may negotiate a different buffer size.
// ---------------------------------------------------------------------------
struct DeviceSpec
{
    std::uint32_t speed         = static_cast<std::uint32_t>( ::xash::limits::sound_dma_speed );
    std::uint8_t  channels      = 2;  // interleaved stereo (SND-OQ-5)
    std::uint8_t  width         = 2;  // bytes/sample: 16-bit
    std::size_t   buffer_frames = 0;  // 0 = backend default (one frame = one stereo pair)
};

struct DeviceCaps
{
    std::uint32_t speed         = 0;
    std::uint8_t  channels      = 0;
    std::uint8_t  width         = 0;
    std::size_t   buffer_frames = 0;
};

// ---------------------------------------------------------------------------
// IAudioFillSource — the pull target the device's audio callback invokes.
// Replaces the SDL audio callback's read of snd.buffer.
//
// @thread-safety: fill() runs on T_AudioCallback (real-time).  Fill `out` with
// interleaved-stereo int16 frames (out.size() == 2 * frame_count).  MUST NOT
// allocate, lock, or block.  Under-supply is silence, not an error — the caller
// zero-fills any remainder it did not write.
// ---------------------------------------------------------------------------
class IAudioFillSource
{
public:
    IAudioFillSource() noexcept                              = default;
    virtual ~IAudioFillSource()                             = default;
    IAudioFillSource( const IAudioFillSource & )            = delete;
    IAudioFillSource &operator=( const IAudioFillSource & ) = delete;

    virtual void fill( std::span<std::int16_t> out ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// IAudioDevice — the output-device seam (an I<X> is earned here: test fakes +
// production null/real variants, and the P-2 device-consumer service pattern).
//
// @thread-safety: all four methods are T_Main lifecycle calls; the device
// drives fill() on its own real-time thread once set_active(true) is called.
// ---------------------------------------------------------------------------
class IAudioDevice
{
public:
    IAudioDevice() noexcept                          = default;
    virtual ~IAudioDevice()                          = default;
    IAudioDevice( const IAudioDevice & )             = delete;
    IAudioDevice &operator=( const IAudioDevice & )  = delete;

    // SNDDMA_Init: open the device against `spec`; returns the negotiated caps.
    [[nodiscard]] virtual Result<DeviceCaps> open( const DeviceSpec &spec ) noexcept = 0;
    // SNDDMA_Shutdown: release the device.  Idempotent.
    virtual void close() noexcept = 0;
    // SNDDMA_Activate: gate output on focus/pause without tearing down.
    virtual void set_active( bool active ) noexcept = 0;
    // Register the pull target the callback fills from (nullptr detaches -> silence).
    virtual void set_fill_source( IAudioFillSource *source ) noexcept = 0;

    // Does this backend drive its OWN audio callback?  This is the device's
    // half of the Q-23 pull inversion, and it is a CORRECTNESS input, not a
    // hint (S9.7b gate finding CONC-6):
    //   false (default) — the device never invokes fill() by itself, so the
    //     topology must spawn an internal pump thread to consume the PCM ring
    //     (NullDevice; anything that would otherwise let the ring fill up).
    //   true            — the device calls fill() itself: a real SDL/OS backend
    //     from its callback thread, or SinkDevice synchronously from pump().
    //     The topology must NOT spawn its own pump, because that would give the
    //     single-consumer SPSC ring a SECOND reader — which trips SpscRing's
    //     own single-consumer assert in a debug build and is a silent data race
    //     in a release one.
    // `AudioTopology`'s `TopologyParams::internal_pump` is derived from this
    // (sound.cpp) instead of being hardcoded per call site.
    [[nodiscard]] virtual bool drives_own_callback() const noexcept { return false; }
};

// ---------------------------------------------------------------------------
// NullDevice — the ratified "null audio device only" production target.
//
// Semantics (decided per sound-boundary.md, SND-OQ-2 topology symmetry): UNLIKE
// legacy s_stub whose SNDDMA_Init returns false, NullDevice OPENS SUCCESSFULLY
// (returns valid caps mirroring the request) and runs a full lifecycle, but
// produces NO output — it never invokes the fill source.  Rationale: under the
// T_Main -> ... -> T_AudioCallback topology the pipeline runs continuously off
// its own clock regardless of real hardware; a null device that opens cleanly
// lets the whole engine (and headless determinism tests) run with no sound card
// present.  "null = no-output" (silent sink); the topology-symmetry Activate
// that s_stub omits is provided here as a no-op.
// ---------------------------------------------------------------------------
class NullDevice final : public IAudioDevice
{
public:
    [[nodiscard]] Result<DeviceCaps> open( const DeviceSpec &spec ) noexcept override;
    void close() noexcept override;
    void set_active( bool active ) noexcept override;
    void set_fill_source( IAudioFillSource *source ) noexcept override;

    [[nodiscard]] bool is_open()   const noexcept { return open_; }
    [[nodiscard]] bool is_active() const noexcept { return active_; }

private:
    bool open_   = false;
    bool active_ = false;
    // @lifetime: borrowed source pointer (accepted for API symmetry; never
    //   invoked — NullDevice produces no output).
    IAudioFillSource *source_ = nullptr;
};

// ---------------------------------------------------------------------------
// SinkDevice — the ratified virtually-clocked TEST consumer (the S9.8
// determinism witness).  It is NOT wall-clock paced: nothing runs on a real
// audio thread.  pump(n_frames) synchronously PULLS exactly n_frames from the
// registered fill source into an accessible interleaved-stereo int16 buffer the
// test inspects.  With no source registered (or while inactive) pump() yields
// silence.  No sleeps, no threads — deterministic by construction.
// ---------------------------------------------------------------------------
class SinkDevice final : public IAudioDevice
{
public:
    [[nodiscard]] Result<DeviceCaps> open( const DeviceSpec &spec ) noexcept override;
    void close() noexcept override;
    void set_active( bool active ) noexcept override;
    void set_fill_source( IAudioFillSource *source ) noexcept override;

    // pump() IS this device's audio callback — it invokes fill() directly on
    // whatever thread called it (which must therefore have registered
    // ThreadRole::AudioCallback).  So the topology must not also run an
    // internal pump: see IAudioDevice::drives_own_callback (CONC-6).
    [[nodiscard]] bool drives_own_callback() const noexcept override { return true; }

    // Pull exactly `n_frames` stereo frames through the fill source into the
    // internal buffer (cleared first).  Returns a view of the pulled samples
    // (size == 2 * n_frames).  Virtual clock: no wall-clock pacing.
    [[nodiscard]] std::span<const std::int16_t> pump( std::size_t n_frames );

    [[nodiscard]] bool          is_open()   const noexcept { return open_; }
    [[nodiscard]] bool          is_active() const noexcept { return active_; }
    [[nodiscard]] std::uint64_t frames_pumped() const noexcept { return frames_pumped_; }

private:
    bool          open_          = false;
    bool          active_        = false;
    std::uint64_t frames_pumped_ = 0;
    // @lifetime: borrowed source pointer, owned by the mixer/test; must outlive
    //   any pump() call.
    IAudioFillSource     *source_ = nullptr;
    std::vector<std::int16_t> buffer_; // interleaved-stereo scratch (grown on demand)
};

} // namespace xash::sound
