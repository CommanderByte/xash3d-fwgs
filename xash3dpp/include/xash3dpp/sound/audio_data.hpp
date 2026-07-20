#pragma once
// xash3dpp — decoded-audio value type (Chunk 9, slice S9.2)
// Legacy reference: engine/common/common.h `wavdata_t` (:534-545), populated
// by SoundPack() (engine/client/soundlib/snd_main.c:30-48) from the per-load
// file-scope `sound` (sndlib_t) global (engine/common/soundlib/snd_utils.c:19,
// soundlib.h:41-59). AudioData is the rewrite's OWNED, non-file-scope
// equivalent: IAudioCodec::decode() (private/sound/codec.hpp) returns exactly
// one AudioData per call — never a process-wide shared/static object. The
// deep-dive flags `sound` (sndlib_t) and snd_wav.c's `iff_*` statics as the
// Race-shared soundlib hazards this type (plus a local parse cursor —
// codec_wav.cpp) exists to remove (sound-boundary.md §Owned state, §Threading).
//
// ---------------------------------------------------------------------------
// Field map: legacy wavdata_t -> AudioData (verified against SoundPack()):
// ---------------------------------------------------------------------------
//   size            -> buffer.size()   (derived; not stored twice, unlike
//                       legacy's separate bounds-check field)
//   loop_start      -> loop_start      (uint; NOT divided by channels — ported
//                       verbatim from the cue chunk's raw dwSampleOffset, see
//                       codec_wav.cpp; legacy never rescales it either)
//   samples         -> samples         (uint; PER-CHANNEL frame count, i.e.
//                       already divided by channels — matches legacy's
//                       `sound.samples /= sound.channels` in Sound_LoadWAV)
//   type            -> type            (sndformat_t -> AudioFormatType, same
//                       ordinal order)
//   flags           -> flags           (sndFlags_t -> AudioFlags; WAV decode
//                       only ever sets Looped — Stream/Resample are
//                       Sound_Process/streaming-registration flags, never
//                       decode output; declared here for documentation parity)
//   rate            -> rate            (WIDENED: legacy `word rate` is
//                       uint16_t, truncating sndlib_t::rate's 32-bit
//                       GetLittleLong() file read at SoundPack() time.
//                       AudioData keeps the full 32-bit value read from the
//                       file. NOTE: rates >= 65536 (88.2k/96k/176.4k/192k)
//                       DO exist — legacy stores rate & 0xFFFF for them
//                       (96000 -> 30464) and mis-resamples; the port keeps
//                       the true rate. A deliberate bug-fix-class deviation
//                       for high-rate WAVs; identical for every rate the
//                       GoldSrc-era assets actually use — documented, NOT
//                       reproduced. Parity-audit rationale fix 2026-07-20.)
//   width           -> width           (byte; 1 or 2 — bytes per sample)
//   channels        -> channels        (byte; 1 or 2)
//   buffer[]         -> buffer          (owned; std::vector<std::byte>, holds
//                       exactly `samples * width * channels` bytes)
//
// @thread-safety: a plain value type — no shared state; safe to construct,
// copy (implicitly via move — see below), or destroy on any thread. Produced
// by IAudioCodec::decode(), which is itself stateless/pure (codec.hpp) — the
// whole decode path is reentrant and thread-safe by construction, even though
// nothing calls it off T_Main yet in this slice (S9.2 wiring is deferred to
// S9.3).

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xash::sound {

// legacy: engine/common/common.h `sndformat_e` (WF_UNKNOWN..WF_OPUSDATA,
// :513-521). Only Pcm is ever produced this slice (WavCodec); Mpeg/Vorbis/
// Opus are reserved for the mp3/ogg/opus satellite targets (boundary Q-11 —
// "ogg/opus pass Q-11 cleanly", "mp3/libmpg... flagged for adjudication").
enum class AudioFormatType : std::uint32_t
{
    Unknown = 0, // WF_UNKNOWN
    Pcm,         // WF_PCMDATA    — WavCodec (this slice)
    Mpeg,        // WF_MPGDATA    — reserved, mp3 satellite (not linked)
    Vorbis,      // WF_VORBISDATA — reserved, ogg satellite (not linked)
    Opus,        // WF_OPUSDATA   — reserved, opus satellite (not linked)
};

// legacy: engine/common/common.h `sndFlags_e` (wavdata_t->flags bits, :524-532).
enum class AudioFlags : std::uint32_t
{
    None     = 0,
    Looped   = 1u << 0,  // SOUND_LOOPED   — a cue chunk supplied a loop point
    Stream   = 1u << 1,  // SOUND_STREAM   — streaminfo, not decode output (reserved)
    Resample = 1u << 12, // SOUND_RESAMPLE — Sound_Process manipulation flag (reserved; never decode output)
};

[[nodiscard]] constexpr AudioFlags operator|( AudioFlags a, AudioFlags b ) noexcept
{
    return static_cast<AudioFlags>( static_cast<std::uint32_t>( a ) | static_cast<std::uint32_t>( b ) );
}
[[nodiscard]] constexpr AudioFlags operator&( AudioFlags a, AudioFlags b ) noexcept
{
    return static_cast<AudioFlags>( static_cast<std::uint32_t>( a ) & static_cast<std::uint32_t>( b ) );
}
[[nodiscard]] constexpr AudioFlags operator~( AudioFlags a ) noexcept
{
    return static_cast<AudioFlags>( ~static_cast<std::uint32_t>( a ) );
}
constexpr AudioFlags &operator|=( AudioFlags &a, AudioFlags b ) noexcept { return a = a | b; }
constexpr AudioFlags &operator&=( AudioFlags &a, AudioFlags b ) noexcept { return a = a & b; }

// True if every bit in |mask| is set in |flags|.
[[nodiscard]] constexpr bool has_flag( AudioFlags flags, AudioFlags mask ) noexcept
{
    return ( flags & mask ) == mask;
}

// ---------------------------------------------------------------------------
// AudioData — a decoded sound (the wavdata_t equivalent). See the field-map
// comment at the top of this file for the legacy correspondence.
// ---------------------------------------------------------------------------
struct AudioData
{
    std::uint32_t   rate       = 0;                      // samples/sec (see field-map: widened vs. legacy `word`)
    std::uint8_t    width      = 0;                       // bytes/sample: 1 or 2
    std::uint8_t    channels   = 0;                       // 1 (mono) or 2 (stereo)
    std::uint32_t   loop_start = 0;                        // loop start, in samples (NOT divided by channels)
    std::uint32_t   samples    = 0;                        // PER-CHANNEL frame count
    AudioFormatType type       = AudioFormatType::Unknown;
    AudioFlags      flags      = AudioFlags::None;
    std::vector<std::byte> buffer;                          // owned PCM bytes; size() == samples*width*channels

    [[nodiscard]] bool empty() const noexcept { return buffer.empty(); }
};

} // namespace xash::sound
