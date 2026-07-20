#pragma once
// xash3dpp — Sound: audio engine core (Chunk 9, slice S9.1 — seams + types).
// Legacy reference: engine/client/sound/ (s_main.c S_Init/S_Shutdown lifecycle,
// the `snd` global s_main.c:39) + engine/platform/ (the SNDDMA seam).
// Boundary spec: docs/boundaries/sound-boundary.md (authoritative surface).
// Decision refs:
//   • Q-22 LIFECYCLE_MODEL — Sound is a pimpl class with an init/shutdown RAII
//     lifecycle; create_sound() is the convenience factory.
//   • Q-23 (device pull-shape) — output goes through the IAudioDevice seam.
//   • SND-OQ-1 — cross-thread listener/entity/gate data via P-2 snapshots +
//     the IEntitySpatialProvider / IMouthSink seams (providers.hpp).
//   • SND-OQ-5 — interleaved-stereo int16 ring payload.
//
// This slice wires ONLY the lifecycle + device open/close.  No mixer, no codec,
// no channel plumbing (S9.2/S9.3).  The class is EngineContext-embeddable
// (direct member, like Server/Host) AND constructible via create_sound().

#include <xash3dpp/sound/device.hpp>
#include <xash3dpp/sound/errors.hpp>
#include <xash3dpp/sound/providers.hpp>

#include <atomic>
#include <cstdint>
#include <memory>

namespace xash::filesystem { class Filesystem; }

namespace xash::sound {

// ---------------------------------------------------------------------------
// SoundStats (P-2) — mix-block counters published as whole-struct relaxed
// atomics; any-thread readers (future debug thread, G-3) snapshot-read without
// touching live channel/mix state (three-tier model, debug-stats-design.md).
// The underrun counter is Tier-1 always-on per threading-model §5.2.
// ---------------------------------------------------------------------------
struct SoundStats
{
    // ---- Tier 1 — always-on ---------------------------------------------
    std::atomic<std::uint64_t> mix_blocks      { 0 }; // paint blocks completed
    std::atomic<std::uint64_t> underruns       { 0 }; // ring underruns (§5.2 always-on)
    std::atomic<std::uint32_t> active_channels { 0 }; // channels mixed last block
    std::atomic<std::uint32_t> dropped_sounds  { 0 }; // start requests dropped (queue full / no channel)
    std::atomic<std::int32_t>  dsp_room        { 0 }; // current DSP room index (idsp_room)

#if XASH_STATS
    // ---- Tier 2 — bookkeeping (profiling builds) ------------------------
    std::atomic<std::uint32_t> peak_active_channels { 0 };
#endif

#if XASH_DEBUG_SOUND
    // ---- Tier 3 — dev-only heavy tracing (not yet defined) --------------
#endif
};

// ---------------------------------------------------------------------------
// SoundInitParams (Q-4) — injected dependencies + init-time config.  All
// pointers are non-owning and must outlive the Sound.
// ---------------------------------------------------------------------------
struct SoundInitParams
{
    // Output device (Q-23).  nullptr -> Sound owns an internal NullDevice (the
    // ratified "null audio device only" fallback); tests inject a SinkDevice.
    IAudioDevice *device = nullptr;            // @lifetime: caller (outlives Sound)

    // SND-OQ-1 seams.  nullptr on a headless / test load: no spatialization /
    // no mouth write-back, but the lifecycle still runs.
    IEntitySpatialProvider *spatial = nullptr; // @lifetime: caller (outlives Sound)
    IMouthSink             *mouth   = nullptr; // @lifetime: caller (outlives Sound)

    // Requested output format (defaults to 44.1 kHz / stereo / 16-bit).
    DeviceSpec spec {};

    // TODO(S9.2/S9.3): filesystem for soundlib decode (WAV/MP3/OGG/Opus) + the
    //   cmd_cvar context for the 11 cvars / 13 commands S_Init registers.
    ::xash::filesystem::Filesystem *filesystem = nullptr; // @lifetime: caller
};

// ---------------------------------------------------------------------------
// Sound — the audio engine (pimpl).
//
// @thread-safety: init/shutdown and every public entry run on ThreadRole::Main
// and assert it (T_Main; QN).  The device drives the mix off its own real-time
// thread once running (S9.2+).  The sole any-thread surface is stats(): the
// SoundStats Tier-1 counters are std::atomic and readable from any thread.
// ---------------------------------------------------------------------------
class Sound
{
public:
    Sound();
    ~Sound();

    Sound( const Sound & )            = delete;
    Sound &operator=( const Sound & ) = delete;

    Sound( Sound && ) noexcept;
    Sound &operator=( Sound && ) noexcept;

    // Lifecycle — main-thread only.  Opens the (injected or fallback null)
    // device.  Rejects a second init() without an intervening shutdown()
    // (SoundError::AlreadyInitialized).  Propagates the device open failure.
    [[nodiscard]] Result<void> init( const SoundInitParams &params );
    void                       shutdown();

    [[nodiscard]] bool initialized() const noexcept;

    [[nodiscard]] const SoundStats &stats() const noexcept;

    // The active output device (the injected one, or the owned NullDevice
    // fallback).  Null before init() / after shutdown().
    [[nodiscard]] IAudioDevice     *device() const noexcept;
    // The format negotiated at open time.  Zero-valued before init().
    [[nodiscard]] const DeviceCaps &caps() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------------------
// create_sound (Q-22 factory) — construct a Sound and init() it.  Returns the
// owned instance on success, or nullptr if init() failed (the reason is logged
// via core::log by init()).
// ---------------------------------------------------------------------------
[[nodiscard]] std::unique_ptr<Sound> create_sound( const SoundInitParams &params );

} // namespace xash::sound
