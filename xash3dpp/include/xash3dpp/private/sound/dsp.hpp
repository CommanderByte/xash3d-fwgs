#pragma once
// xash3dpp — room-effects DSP (Chunk 9, slice S9.5 = SX_RoomFX). PARITY-CRITICAL.
// Legacy reference: engine/client/sound/s_dsp.c (ALL of it — both 29-row preset
// tables, dsp_coeff_table selection, SX_Init/SX_ClearState/SX_Free lifecycle,
// the delay-line buffer allocations, room selection (idsp_room, waterlevel vs
// room_type/waterroom_type), the four passes (RVB_DoAMod, RVB_DoReverb,
// DLY_DoDelay, DLY_DoStereoDelay) and their exact integer math, the cvar
// surface, SX_Profiling_f).
//
// Boundary spec: docs/boundaries/sound-boundary.md §Quirks ("DSP off-by-one —
// idsp_room==29 vs 28-max table"), §Owned state (idsp_room + all s_dsp.c
// file-scope statics -> "selection on T_Main, processing on T_AudioDecoder"),
// §Extension axes P-7 (pool-owned RAII door-debt), §Threading (SND-OQ-1: the
// waterlevel room-selection input crosses via ListenerSnapshot, never a live
// cl.local.waterlevel read on the mix thread).
//
// SND-OQ-6 resolution (decided this slice, 2026-07-20): REPRODUCE-WITH-
// DEFINED-BEHAVIOUR. Legacy's `bound(0, idsp_room, MAX_ROOM_TYPES)` clamp is
// INCLUSIVE (s_dsp.c:809) against `MAX_ROOM_TYPES = ARRAYSIZE(rgsxpre) = 29`,
// permitting idsp_room == 29 to index a 29-element array (valid 0-28) — an
// out-of-bounds read whose actual legacy content is whatever bytes happen to
// follow the static array in the compiled binary's data segment: build-,
// compiler- and optimisation-level-dependent, not a reproducible or
// meaningful value. Per the task brief's own fallback ("if the padded-row
// contents cannot be made meaningful, a zeroed row with a comment is the
// defined superset"), both tables below are padded to k_room_preset_count
// (30) rows with an all-zero RoomPreset at index 29 (k_max_room_types).
// k_max_room_types (the clamp bound, 29) is kept textually distinct from the
// padded storage size specifically so the clamp formula stays byte-for-byte
// identical to legacy's `bound(0, idsp_room, MAX_ROOM_TYPES)`. A zeroed row
// is behaviourally indistinguishable from preset 0 ("off") for every field
// that affects observable mix output: room_size==0 and room_delay==0 both
// disable their respective delay lines (RVB_CheckNewReverbVal /
// DLY_CheckNewDelayVal's `if (delay == 0) DLY_Free(...)` branches) exactly
// like preset 0, and room_rvblp/room_dlylp (the two fields where the zeroed
// row differs numerically from preset 0's 1.0/2.0) are read ONLY inside the
// lowpass branch of an ACTIVE delay line — which never runs once its
// governing delay/size is 0. See test_sound_dsp.cpp's SND-OQ-6 case for the
// pinned equivalence. Decision recorded in decisions-architecture.md §3a.
//
// @thread-safety: mirrors the Mixer/VoxSystem posture (mixer.hpp/vox.hpp) —
// RoomDsp is confined to whichever thread OWNS THE CHANNEL ARRAY: it holds the
// delay lines process() mutates in place from Mixer::paint_channels(), and the
// cvar-equivalent setters retune those same lines. That owner is
// T_AudioDecoder while the S9.7b topology runs and T_Main when it does not.
//
// compliance-allow(thread-assert): the role is CONDITIONAL (see above), so no
// single per-call assert can state it — AudioDecoder would fire on every
// legitimate topology-off call and Main on every threaded one. It is enforced
// at the entry points that know the mode: every RoomDsp call in the tree is
// reached either through apply_command() (audio_command.cpp: set_waterlevel /
// the §4.3 setters / clear_state), whose two callers assert Main
// (Sound::*, sound.cpp) or AudioDecoder (AudioTopology::decoder_step(),
// topology.cpp), or through Mixer::paint_channels(), which itself asserts
// AudioDecoder. A redundant per-call assert underneath those gates would buy
// nothing and could not encode the mode.
// (The S9.5 rationale — "no decoder thread exists until S9.7b" — EXPIRED with
// this slice and has been replaced by the above.)
//
// ONE EXCEPTION, and it is gated rather than papered over: profile()
// (SX_Profiling_f) is reachable from a T_Main console command, which is NOT
// the channel-array owner while the topology runs. Sound::cmd_dsp_profile_f
// therefore REFUSES the command whenever topology_running() (parity F-3 /
// concurrency CONC-2 — profile() rewrites every delay line and would race the
// decoder painting through the same object). See sound.cpp.

#include <xash3dpp/private/sound/mixer.hpp> // IRoomDsp, portable_samplepair_t (via mix_kernels.hpp)

#include <xash3dpp/memory/memory.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace xash::sound {

// ---------------------------------------------------------------------------
// Named arithmetic-law constants (mirrors mixer.hpp's "kept here, not in
// limits.hpp" pattern — these are parity-critical clamp bounds/ring-buffer
// sizing constants, not tunable budgets).
// ---------------------------------------------------------------------------

// MAX_ROOM_TYPES = ARRAYSIZE(rgsxpre) (s_dsp.c:21) — the LEGACY table size
// (29) and the clamp's inclusive upper bound. Deliberately distinct from
// k_room_preset_count (our padded storage) — see the SND-OQ-6 note above.
inline constexpr int k_max_room_types = 29;

// Padded storage size: k_max_room_types rows (0-28, legacy content) plus the
// SND-OQ-6 sentinel row at index 28's successor, i.e. index k_max_room_types
// (29) itself.
inline constexpr std::size_t k_room_preset_count = static_cast<std::size_t>( k_max_room_types ) + 1;

inline constexpr float k_max_mono_delay   = 0.4f;  // MAX_MONO_DELAY (s_dsp.c:22)
inline constexpr float k_max_reverb_delay = 0.1f;  // MAX_REVERB_DELAY (s_dsp.c:23)
inline constexpr int   k_reverb_xfade     = 32;     // REVERB_XFADE (s_dsp.c:24)
inline constexpr float k_max_stereo_delay = 0.1f;  // MAX_STEREO_DELAY (s_dsp.c:25)
inline constexpr int   k_max_lp           = 10;     // MAXLP (s_dsp.c:26)

// idsp_dma_speed (s_dsp.c:171): set to SOUND_11k once in SX_Init and NEVER
// re-queried afterwards — "dead scaling generality" per the deep-dive (R9.4
// s_dsp.c:503). Modelled as a compile-time constant rather than a member for
// exactly that reason: no code path in s_dsp.c ever assigns it a second
// value.
inline constexpr int k_idsp_dma_speed = 11025; // SOUND_11k (engine/client/sound.h:25)
inline constexpr int k_sound_11k      = 11025; // SOUND_11k — used only in the (kmod * dma/SOUND_11k) ratio, always 1

// ---------------------------------------------------------------------------
// RoomPreset — sx_preset_t (s_dsp.c:28-43). Field order preserved verbatim
// (matches both preset tables' column order exactly).
// ---------------------------------------------------------------------------
struct RoomPreset
{
    float room_lp       = 0.0f; // lowpass
    float room_mod      = 0.0f; // modulation
    float room_size     = 0.0f; // reverb: initial reflection size
    float room_refl     = 0.0f; // reverb: decay time (feedback)
    float room_rvblp    = 0.0f; // reverb: low pass filtering level
    float room_delay    = 0.0f; // mono delay: delay time
    float room_feedback = 0.0f; // mono delay: decay time
    float room_dlylp    = 0.0f; // mono delay: low pass filtering level
    float room_left     = 0.0f; // stereo delay: left channel delay time
};

// rgsxpre (s_dsp.c:72-105) — the release preset table, PLUS the SND-OQ-6
// padded sentinel row at index k_max_room_types (29). Rows 0-28 transcribed
// verbatim from source (row comments preserved for cross-reference).
inline constexpr std::array<RoomPreset, k_room_preset_count> k_room_presets_release { {
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.0f,   0.0f,   2.0f, 0.0f   }, // 0 off
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.065f, 0.1f,   0.0f, 0.01f  }, // 1 generic
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.02f,  0.75f,  0.0f, 0.01f  }, // 2 metalic
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.03f,  0.78f,  0.0f, 0.02f  }, // 3
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.06f,  0.77f,  0.0f, 0.03f  }, // 4
    { 0.0f, 0.0f, 0.05f,  0.85f,  1.0f, 0.008f, 0.96f,  2.0f, 0.01f  }, // 5 tunnel
    { 0.0f, 0.0f, 0.05f,  0.88f,  1.0f, 0.01f,  0.98f,  2.0f, 0.02f  }, // 6
    { 0.0f, 0.0f, 0.05f,  0.92f,  1.0f, 0.015f, 0.995f, 2.0f, 0.04f  }, // 7
    { 0.0f, 0.0f, 0.05f,  0.84f,  1.0f, 0.0f,   0.0f,   2.0f, 0.012f }, // 8 chamber
    { 0.0f, 0.0f, 0.05f,  0.9f,   1.0f, 0.0f,   0.0f,   2.0f, 0.008f }, // 9
    { 0.0f, 0.0f, 0.05f,  0.95f,  1.0f, 0.0f,   0.0f,   2.0f, 0.004f }, // 10
    { 0.0f, 0.0f, 0.05f,  0.7f,   0.0f, 0.0f,   0.0f,   2.0f, 0.012f }, // 11 brite
    { 0.0f, 0.0f, 0.055f, 0.78f,  0.0f, 0.0f,   0.0f,   2.0f, 0.008f }, // 12
    { 0.0f, 0.0f, 0.05f,  0.86f,  0.0f, 0.0f,   0.0f,   2.0f, 0.002f }, // 13
    { 1.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.0f,   0.0f,   2.0f, 0.01f  }, // 14 water
    { 1.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.06f,  0.85f,  2.0f, 0.02f  }, // 15
    { 1.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.2f,   0.6f,   2.0f, 0.05f  }, // 16
    { 0.0f, 0.0f, 0.05f,  0.8f,   1.0f, 0.0f,   0.48f,  2.0f, 0.016f }, // 17 concrete
    { 0.0f, 0.0f, 0.06f,  0.9f,   1.0f, 0.0f,   0.52f,  2.0f, 0.01f  }, // 18
    { 0.0f, 0.0f, 0.07f,  0.94f,  1.0f, 0.3f,   0.6f,   2.0f, 0.008f }, // 19
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.3f,   0.42f,  2.0f, 0.0f   }, // 20 outside
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.35f,  0.48f,  2.0f, 0.0f   }, // 21
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.38f,  0.6f,   2.0f, 0.0f   }, // 22
    { 0.0f, 0.0f, 0.05f,  0.9f,   1.0f, 0.2f,   0.28f,  0.0f, 0.0f   }, // 23 cavern
    { 0.0f, 0.0f, 0.07f,  0.9f,   1.0f, 0.3f,   0.4f,   0.0f, 0.0f   }, // 24
    { 0.0f, 0.0f, 0.09f,  0.9f,   1.0f, 0.35f,  0.5f,   0.0f, 0.0f   }, // 25
    { 0.0f, 1.0f, 0.01f,  0.9f,   0.0f, 0.0f,   0.0f,   2.0f, 0.05f  }, // 26 weirdo
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.009f, 0.999f, 2.0f, 0.04f  }, // 27
    { 0.0f, 0.0f, 0.001f, 0.999f, 0.0f, 0.2f,   0.8f,   2.0f, 0.05f  }, // 28
    { 0.0f, 0.0f, 0.0f,   0.0f,   0.0f, 0.0f,   0.0f,   0.0f, 0.0f   }, // 29 SND-OQ-6 sentinel (padded, zeroed — see file header)
} };

// rgsxpre_hlalpha052 (s_dsp.c:109-142; 0x0045dca8 enginegl.exe,
// SHA256:42383d32cd712e59ee2c1bd78b7ba48814e680e7026c4223e730111f34a60d66),
// PLUS the same SND-OQ-6 padded sentinel row at index k_max_room_types.
inline constexpr std::array<RoomPreset, k_room_preset_count> k_room_presets_hlalpha052 { {
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.0f,   0.0f,   2.0f, 0.0f   }, // 0 off
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.08f,  0.8f,   2.0f, 0.0f   }, // 1 generic
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.02f,  0.75f,  0.0f, 0.001f }, // 2 metalic
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.03f,  0.78f,  0.0f, 0.002f }, // 3
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.06f,  0.77f,  0.0f, 0.003f }, // 4
    { 0.0f, 0.0f, 0.05f,  0.85f,  1.0f, 0.008f, 0.96f,  2.0f, 0.01f  }, // 5 tunnel
    { 0.0f, 0.0f, 0.05f,  0.88f,  1.0f, 0.01f,  0.98f,  2.0f, 0.02f  }, // 6
    { 0.0f, 0.0f, 0.05f,  0.92f,  1.0f, 0.015f, 0.995f, 2.0f, 0.04f  }, // 7
    { 0.0f, 0.0f, 0.05f,  0.84f,  1.0f, 0.0f,   0.0f,   2.0f, 0.003f }, // 8 chamber
    { 0.0f, 0.0f, 0.05f,  0.9f,   1.0f, 0.0f,   0.0f,   2.0f, 0.002f }, // 9
    { 0.0f, 0.0f, 0.05f,  0.95f,  1.0f, 0.0f,   0.0f,   2.0f, 0.001f }, // 10
    { 0.0f, 0.0f, 0.05f,  0.7f,   0.0f, 0.0f,   0.0f,   2.0f, 0.003f }, // 11 brite
    { 0.0f, 0.0f, 0.055f, 0.78f,  0.0f, 0.0f,   0.0f,   2.0f, 0.002f }, // 12
    { 0.0f, 0.0f, 0.05f,  0.86f,  0.0f, 0.0f,   0.0f,   2.0f, 0.001f }, // 13
    { 1.0f, 1.0f, 0.0f,   0.0f,   1.0f, 0.0f,   0.0f,   2.0f, 0.01f  }, // 14 water
    { 1.0f, 1.0f, 0.0f,   0.0f,   1.0f, 0.06f,  0.85f,  2.0f, 0.02f  }, // 15
    { 1.0f, 1.0f, 0.0f,   0.0f,   1.0f, 0.2f,   0.6f,   2.0f, 0.05f  }, // 16
    { 0.0f, 0.0f, 0.05f,  0.8f,   1.0f, 0.15f,  0.48f,  2.0f, 0.008f }, // 17 concrete
    { 0.0f, 0.0f, 0.06f,  0.9f,   1.0f, 0.22f,  0.52f,  2.0f, 0.005f }, // 18
    { 0.0f, 0.0f, 0.07f,  0.94f,  1.0f, 0.3f,   0.6f,   2.0f, 0.001f }, // 19
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.3f,   0.42f,  2.0f, 0.0f   }, // 20 outside
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.35f,  0.48f,  2.0f, 0.0f   }, // 21
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.38f,  0.6f,   2.0f, 0.0f   }, // 22
    { 0.0f, 0.0f, 0.05f,  0.9f,   1.0f, 0.2f,   0.28f,  0.0f, 0.0f   }, // 23 cavern
    { 0.0f, 0.0f, 0.07f,  0.9f,   1.0f, 0.3f,   0.4f,   0.0f, 0.0f   }, // 24
    { 0.0f, 0.0f, 0.09f,  0.9f,   1.0f, 0.35f,  0.5f,   0.0f, 0.0f   }, // 25
    { 0.0f, 1.0f, 0.01f,  0.9f,   0.0f, 0.0f,   0.0f,   2.0f, 0.05f  }, // 26 weirdo
    { 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.009f, 0.999f, 2.0f, 0.04f  }, // 27
    { 0.0f, 0.0f, 0.001f, 0.999f, 0.0f, 0.2f,   0.8f,   2.0f, 0.05f  }, // 28
    { 0.0f, 0.0f, 0.0f,   0.0f,   0.0f, 0.0f,   0.0f,   0.0f, 0.0f   }, // 29 SND-OQ-6 sentinel (padded, zeroed — see file header)
} };

// select_room — the T_Main-computable half of SX_CheckPresets (s_dsp.c:806,
// 809): `idsp_room = waterlevel>2 ? waterroom_type : room_type; idsp_room =
// bound(0, idsp_room, MAX_ROOM_TYPES)`. A PURE function of its three
// arguments (no RoomDsp instance state) — proves selection needs nothing
// beyond ListenerSnapshot::waterlevel + the two room-type cvar values, so it
// can run on T_Main; only the returned index need cross to the mix thread
// (sound-boundary.md §Owned state, idsp_room row). The float->int truncation
// matches legacy's implicit `int idsp_room = ...float cvar value...` assign.
[[nodiscard]] constexpr int select_room( int waterlevel, float room_type, float waterroom_type ) noexcept
{
    const int raw = waterlevel > 2 ? static_cast<int>( waterroom_type ) : static_cast<int>( room_type );
    // bound(min,num,max) verbatim: num >= min ? (num < max ? num : max) : min — `<` not `<=`,
    // so max (29) IS reachable (the SND-OQ-6 off-by-one, preserved not fixed).
    return raw >= 0 ? ( raw < k_max_room_types ? raw : k_max_room_types ) : 0;
}

// ---------------------------------------------------------------------------
// PoolIntBuffer — RAII wrapper over a pool-allocated int[] (P-7: promotes
// legacy's raw `Mem_Calloc(sndpool, cdelaysamplesmax * sizeof(int))` /
// `Mem_Free(dly->lpdelayline)` pair — DLY_Init/DLY_Free, s_dsp.c:262-269,
// 296-313 — to pool-owned RAII, per the boundary's P-7 door-debt note: "the
// rewrite should adopt the RAII idiom for the Sound/soundlib pool ownership
// at port time." Move-only. mem_calloc zero-initialises, matching
// Mem_Calloc's contract exactly (DLY_Init relies on a zero-filled line).
// ---------------------------------------------------------------------------
class PoolIntBuffer
{
public:
    PoolIntBuffer() noexcept = default;
    ~PoolIntBuffer() noexcept { reset(); }

    PoolIntBuffer( const PoolIntBuffer & )            = delete;
    PoolIntBuffer &operator=( const PoolIntBuffer & ) = delete;

    PoolIntBuffer( PoolIntBuffer &&other ) noexcept
        : pool_( other.pool_ ), data_( other.data_ ), size_( other.size_ )
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    PoolIntBuffer &operator=( PoolIntBuffer &&other ) noexcept
    {
        if( this != &other )
        {
            reset();
            pool_       = other.pool_;
            data_       = other.data_;
            size_       = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // DLY_Free (s_dsp.c:262-269): free + null. No-op if already inactive.
    void reset() noexcept
    {
        if( data_ != nullptr )
        {
            ::xash::memory::mem_free( data_ );
            data_ = nullptr;
        }
        size_ = 0;
    }

    // The Mem_Calloc half of DLY_Init (s_dsp.c:301). Frees any prior
    // allocation first (matches DLY_Init's own leading DLY_Free(cur) call).
    // Returns false on OOM (buffer left inactive, matching a legacy
    // Mem_Calloc failure — unchecked in the original, but reproducing the
    // crash is not a goal; false lets the caller fail safe instead).
    [[nodiscard]] bool allocate( ::xash::memory::PoolHandle pool, std::size_t count ) noexcept
    {
        reset();
        pool_       = pool;
        void *block = ::xash::memory::mem_calloc( pool, count * sizeof( int ) );
        if( block == nullptr )
            return false;
        data_ = static_cast<int *>( block );
        size_ = count;
        return true;
    }

    [[nodiscard]] bool active() const noexcept { return data_ != nullptr; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    [[nodiscard]] int       &operator[]( std::size_t i ) noexcept { return data_[i]; }
    [[nodiscard]] const int &operator[]( std::size_t i ) const noexcept { return data_[i]; }

    // memset(0) equivalent — DLY_CheckNewDelayVal re-zeroes on a delay-length
    // change without reallocating (s_dsp.c:472).
    void zero() noexcept;

private:
    ::xash::memory::PoolHandle pool_ {};
    int         *data_ = nullptr;
    std::size_t  size_ = 0;
};

// ---------------------------------------------------------------------------
// DelayLine — dly_t (s_dsp.c:45-70). One instance each for monodly,
// reverbdly[0]/[1], stereodly. Field order/types preserved (cdelaysamplesmax/
// idelayinput/idelayoutput stay size_t, matching the legacy underflow-shaped
// arithmetic in DLY_Init/DLY_MovePointer exactly).
// ---------------------------------------------------------------------------
struct DelayLine
{
    PoolIntBuffer buffer; // lpdelayline (nullptr <=> !buffer.active())

    std::size_t cdelaysamplesmax = 0;
    std::size_t idelayinput      = 0;
    std::size_t idelayoutput     = 0;

    int idelayoutputxf = 0;
    int xfade          = 0;

    int delaysamples   = 0;
    int delayfeedback  = 0;

    int lp  = 0;
    int lp0 = 0, lp1 = 0, lp2 = 0;

    int mod    = 0;
    int modcur = 0;

    [[nodiscard]] bool active() const noexcept { return buffer.active(); }
};

// ---------------------------------------------------------------------------
// RoomDsp — RoomDsp : IRoomDsp. Owns the two preset tables' selection state,
// the 14-cvar-equivalent DSP surface (sound-boundary.md §4.3), and the 4
// delay lines. process() is SX_RoomFX; select_room() above is the pure
// T_Main-computable half of SX_CheckPresets, folded into process() via the
// stored waterlevel_/room_type_/waterroom_type_ members set by set_waterlevel/
// set_room_type/set_waterroom_type (the S9.6 wiring point for
// ListenerSnapshot::waterlevel and the room_type/waterroom_type cvars).
// ---------------------------------------------------------------------------
class RoomDsp final : public IRoomDsp
{
public:
    // SX_Init (s_dsp.c:211-253), minus cvar/command REGISTRATION (S9.6 wiring
    // point — see the setters below and profile()). `pool` backs the 4
    // delay-line buffers (P-7; PoolIntBuffer above) — caller-owned, must
    // outlive this RoomDsp (mirrors SaveBuffer's pool-parameter constructor,
    // private/save/save_buffer.hpp).
    explicit RoomDsp( ::xash::memory::PoolHandle pool ) noexcept;
    ~RoomDsp() override = default; // SX_Free minus Cmd_RemoveCommand (S9.6); PoolIntBuffer members self-free.

    RoomDsp( const RoomDsp & )            = delete;
    RoomDsp &operator=( const RoomDsp & ) = delete;

    // --- cvar-equivalent setters (S9.6 wires these to real cvar change
    // callbacks) — sound-boundary.md §4.3's 14-cvar census + waterlevel (the
    // one non-cvar, ListenerSnapshot-sourced input). Plain assignment for the
    // fields legacy reads with NO edge-detection at all (room_off, room_type,
    // waterroom_type, and the 6 preset-derived fields RVB_CheckNewReverbVal/
    // DLY_CheckNewDelayVal refresh unconditionally every call — sxrvb_lp,
    // sxrvb_feedback, sxdly_lp, sxdly_feedback — plus sxmod_lowpass/
    // sxmod_mod, read directly inside RVB_DoAMod every sample). The other 5
    // (dsp_coeff_table, hisound, room_size, room_delay, room_left) track
    // their own CHANGED-equivalent edge exactly like Cvar_DirectSetValue's
    // "only if the value actually differs" contract (cvar.c:607-621,
    // Cvar_SanitizeAndSet's `if (!strcmp(fixed_string, var->string)) return`).
    void set_room_off( bool off ) noexcept { room_off_ = off; }
    void set_room_type( float v ) noexcept { room_type_ = v; }
    void set_waterroom_type( float v ) noexcept { waterroom_type_ = v; }
    void set_waterlevel( int wl ) noexcept { waterlevel_ = wl; } // ListenerSnapshot::waterlevel (SND-OQ-1)
    void set_dsp_coeff_table( float table ) noexcept;
    void set_hisound( int quality ) noexcept;
    void set_room_mod( float v ) noexcept { room_mod_ = v; }
    void set_room_lp( float v ) noexcept { room_lp_ = v; }
    void set_room_rvblp( float v ) noexcept { room_rvblp_ = v; }
    void set_room_refl( float v ) noexcept { room_refl_ = v; }
    void set_room_dlylp( float v ) noexcept { room_dlylp_ = v; }
    void set_room_feedback( float v ) noexcept { room_feedback_ = v; }
    void set_room_size( float v ) noexcept;
    void set_room_delay( float v ) noexcept;
    void set_room_left( float v ) noexcept;

    // idsp_room (s_dsp.c:172) — the ONLY non-static legacy global; read by
    // the s_show debug overlay (s_main.c:1668). Exposed as a P-4-style
    // accessor instead of a bare global.
    [[nodiscard]] int room_index() const noexcept { return idsp_room_; }

    // IRoomDsp (mixer.hpp) — SX_RoomFX (s_dsp.c:846-864): early-out on
    // room_off/0 samples, else SX_CheckPresets -> AMod -> Reverb -> Delay ->
    // StereoDelay, each a full pass over [roombuffer, roombuffer+num_samples).
    void process( portable_samplepair_t *roombuffer, int num_samples ) noexcept override;

    // SX_ClearState (s_dsp.c:868-877): resets room_type to 0 and forces a
    // reload; does NOT free delay lines itself (torn down lazily by the next
    // process() call noticing idsp_room changed — see the .cpp for why this
    // is not a bug, matching the boundary doc's own characterisation).
    void clear_state() noexcept;

    // SX_Profiling_f (s_dsp.c:879-916) as a callable — the `dsp_profile`
    // Cmd_AddRestrictedCommand registration itself is S9.6. `room_type_override`
    // mirrors `Cmd_Argc() > 1` (nullopt == no argument). `now_seconds` replaces
    // Platform_DoubleTime() (testability; @thread-safety: caller's choice).
    struct ProfilingResult
    {
        int    room_type_for_message = 0;   // idsp_room at the point s_dsp.c:899's Con_Printf reads it
        double seconds                = 0.0; // end - start (s_dsp.c:906,908)
    };
    [[nodiscard]] ProfilingResult profile( int calls, std::optional<float> room_type_override,
                                           double ( *now_seconds )() noexcept ) noexcept;

private:
    void check_presets() noexcept;                     // SX_CheckPresets (s_dsp.c:783-844)
    void reload_room_fx() noexcept;                     // SX_ReloadRoomFX (s_dsp.c:196-202)
    void apply_preset( const RoomPreset &preset ) noexcept; // the 9 Cvar_DirectSetValue calls (s_dsp.c:824-832)
    void rvb_check_new_reverb_val() noexcept;            // RVB_CheckNewReverbVal (s_dsp.c:582-604)
    void rvb_set_up_dly( DelayLine &dly, float delay, int kmod ) noexcept; // RVB_SetUpDly (s_dsp.c:546-573)
    void dly_check_new_delay_val() noexcept;             // DLY_CheckNewDelayVal (s_dsp.c:450-487)
    void dly_check_new_stereo_delay_val() noexcept;       // DLY_CheckNewStereoDelayVal (s_dsp.c:338-377)
    void dly_init( DelayLine &dly, float delay_seconds ) noexcept; // DLY_Init (s_dsp.c:296-313)

    // The four passes (s_dsp.c order: AMod, Reverb, Delay, StereoDelay).
    void rvb_do_amod( portable_samplepair_t *paint, int count ) noexcept;    // RVB_DoAMod (s_dsp.c:719-781)
    void rvb_do_reverb( portable_samplepair_t *paint, int count ) noexcept;  // RVB_DoReverb (s_dsp.c:687-710)
    int  rvb_do_reverb_for_one_dly( DelayLine &dly, int vlr, const portable_samplepair_t &sample ) noexcept; // (s_dsp.c:613-678)
    void dly_do_delay( portable_samplepair_t *paint, int count ) noexcept;   // DLY_DoDelay (s_dsp.c:496-537)
    void dly_do_stereo_delay( portable_samplepair_t *paint, int count ) noexcept; // DLY_DoStereoDelay (s_dsp.c:386-441)

    ::xash::memory::PoolHandle pool_;

    // --- cvar-equivalent state (defaults match SX_Init's CVAR_DEFINE*) -----
    bool  room_off_        = false; // "0"
    // Stored as FLOAT (S9.5 parity-audit fix 2026-07-20): legacy reads this
    // cvar with TWO different coercions — `switch((int)value)` for the table
    // pointer (s_dsp.c:787) but an exact-float `value == 1.0f` for the reverb
    // gain (s_dsp.c:703).  A fractional value in (1.0, 2.0) therefore selects
    // the ALPHA table with the RELEASE gain; collapsing to int loses that.
    float dsp_coeff_table_ = 0.0f;  // "0" (0=release, 1=alpha; else release table)
    float room_type_       = 0.0f;  // "0"
    float waterroom_type_  = 14.0f; // "waterroom_type" "14"
    int   hisound_         = 2;     // "room_hires" "2"
    float room_mod_        = 0.0f;  // "room_mod" "0"
    float room_lp_         = 0.0f;  // "room_lp" "0"
    float room_rvblp_      = 1.0f;  // "room_rvblp" "1"
    float room_refl_       = 0.0f;  // "room_refl" "0"
    float room_dlylp_      = 1.0f;  // "room_dlylp" "1"
    float room_feedback_   = 0.2f;  // "room_feedback" "0.2"
    float room_size_       = 0.0f;  // "room_size" "0"
    float room_delay_      = 0.8f;  // "room_delay" "0.8"
    float room_left_       = 0.0f;  // "room_left" "0"
    int   waterlevel_      = 0;     // ListenerSnapshot::waterlevel (not a cvar)

    // Edge-detect bits (FCVAR_CHANGED equivalents) — ONLY for the 5 fields
    // SX_CheckPresets/RVB_*/DLY_* actually branch on. Legacy also sets
    // sxrvb_feedback's and room_type's FCVAR_CHANGED from SX_ReloadRoomFX,
    // but nothing in s_dsp.c ever reads either flag (confirmed by full-file
    // read) — omitted here as a no-observable-effect simplification, not a
    // behaviour change.
    bool dsp_coeff_table_dirty_ = false;
    bool hisound_dirty_         = false;
    bool room_size_dirty_       = false;
    bool room_delay_dirty_      = false;
    bool room_left_dirty_       = false;

    // --- internal DSP state (s_dsp.c file-scope statics) --------------------
    const std::array<RoomPreset, k_room_preset_count> *ptable_ = &k_room_presets_release;
    int idsp_room_     = 0;
    int room_typeprev_ = 0;
    int sxhires_       = 2; // s_dsp.c:226 (set directly in SX_Init, independent of the hisound cvar default)

    // Amplitude-modulation state (RVB_DoAMod). sxmod1_/sxmod2_ are computed
    // once from idsp_dma_speed (fixed, see k_idsp_dma_speed) and never
    // change — which means the `!sxmod1`/`!sxmod2` RNG-trigger branches in
    // rvb_do_amod are UNREACHABLE for any value these members can hold
    // (350/450, both always nonzero). Preserved structurally (see dsp.cpp's
    // random_long stub) rather than special-cased away, per parity-first.
    int sxamodl_ = 255, sxamodr_ = 255, sxamodlt_ = 255, sxamodrt_ = 255;
    int sxmod1cur_ = 350, sxmod2cur_ = 450, sxmod1_ = 350, sxmod2_ = 450;

    std::array<int, static_cast<std::size_t>( k_max_lp )> rgsxlp_ {};

    DelayLine              monodly_;
    std::array<DelayLine, 2> reverbdly_;
    DelayLine              stereodly_;
};

} // namespace xash::sound
