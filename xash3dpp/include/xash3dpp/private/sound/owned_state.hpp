#pragma once
// xash3dpp — Sound-class owned-state stubs (Chunk 9, slice S9.1).
// Typed placeholders for the legacy `snd` global's fields (snd_globals_t,
// s_main.c:39) that become Sound members per sound-boundary.md §Owned state
// disposition.  This slice is SEAMS + TYPES ONLY: these carry no behaviour and
// no live ABI plumbing yet (boundary: "layout-pinned, plumbing deferred").  The
// mixer/clock/channel logic lands in S9.2+.
//
// @thread-safety: internal to the Sound pimpl.  The disposition column records
// each field's eventual thread home (mix-thread-private, MPSC-crossed, ...).
// Sound now runs real T_AudioDecoder/T_AudioCallback threads (S9.7b topology),
// so "no worker threads run in this slice" is stale for the struct as a whole
// — but it is still true for MOST of the individual fields below, which
// remain exactly what their own per-field comments already say: MixClock
// (mix_clock_) and AmbientState (ambient_state_) are pure layout with no
// reader/writer anywhere yet, and ChannelState/RawChannelState
// (channels_/raw_channels_) are explicitly "empty stub" / "reserve deferred"
// — the REAL per-channel mixing array is Mixer's own channels_/raw_channels_
// (mixer.hpp), a distinct, already-wired member. DmaState (dma_) IS populated
// (from DeviceCaps at open/close) but only ever touched on T_Main today.
// FadeState (fade_state_) is the one field that actually crosses threads —
// written by T_Main fade commands, read at paint time via MixGateSnapshot's
// gain param (its own comment below) — so it is the sole candidate here a
// future thread assertion would have anything to guard.

#include <xash3dpp/abi/sound_api.hpp> // channel_t, rawchan_t, snd_format_t, sound_t
#include <xash3dpp/limits.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace xash::sound {

// dma_ — device-facing state (SNDDMA-owned): backend_name/buffer/format/
// initialized/samples/samplepos.  Populated from DeviceCaps at open time.
struct DmaState
{
    ::xash::abi::snd_format_t format {};        // negotiated speed/width/channels
    bool          initialized = false;          // legacy snd.initialized
    int           samples     = 0;              // mono samples in the ring
    int           samplepos   = 0;              // ring write position (mono samples)
};

// mix_clock_ — mix-thread-private under the new topology.  Kept as legacy int
// so the 0x40000000 overflow-reset quirk (s_main.c:1519-1525) ports verbatim
// in S9.2 (do NOT widen without preserving that wrap).
struct MixClock
{
    int painted_time = 0; // total samples mixed at speed
    int sound_time   = 0; // total samples played out at dma speed
};

// channels_ — mutation crosses the P-1 MPSC audio command queue in S9.2.  The
// storage is left empty this slice (reserve deferred with the mixer).
struct ChannelState
{
    // @pre-reserved: sound_max_channels (reserve deferred to S9.2 with the mixer)
    std::vector<::xash::abi::channel_t> channels; // empty stub
    int total_channels = 0;
};

// raw_channels_ — raw/voice streaming channels; same MPSC boundary as channels_.
struct RawChannelState
{
    // @pre-reserved: sound_max_raw_channels (reserve deferred to S9.2)
    std::vector<::xash::abi::rawchan_t *> raw_channels; // empty stub (variable-sized entries)
};

// ambient_state_ — registration-time ambient handles, folded from a
// RegistrationSnapshot (boundary Owned state).
struct AmbientState
{
    std::array<::xash::abi::sound_t, ::xash::abi::k_num_ambients> ambient_sfx {};
    bool have_ambient_sfx = false;
};

// fade_state_ — soundfade: written on T_Main by fade commands, read at
// paint-time; crosses via MixGateSnapshot's gain param (boundary Owned state).
struct FadeState
{
    float percent      = 0.0f; // current fade percentage
    float speed        = 0.0f; // fade rate
    float start_percent = 0.0f;
    bool  active       = false;
};

} // namespace xash::sound
