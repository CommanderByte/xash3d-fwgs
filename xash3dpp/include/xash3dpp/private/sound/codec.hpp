#pragma once
// xash3dpp — sound decode-codec interface (Chunk 9, slice S9.2)
// Legacy reference: engine/common/soundlib/{snd_utils.c (the load_game[] /
// stream_game[] format tables, Sound_SupportedFileFormat :447-459),
// snd_main.c (SoundPack/FS_LoadSound)}, engine/client/soundlib/snd_wav.c (the
// WAV decoder — the only format wired this slice), soundlib.h (loadwavfmt_t /
// streamfmt_t — the legacy v-tables this interface replaces).
//
// Mirrors the IImageCodec precedent (private/imagelib/codec.hpp) byte-for-byte
// in shape: a stateless singleton codec per format, dispatched from a fixed
// registry, plus a SEPARATE extension-only array (the filesystem
// k_archive_types pattern, private/filesystem/archive_registry.hpp) so a
// dedicated-server-style "is this a sound file?" query never links decode
// code — matches legacy's XASH_DEDICATED branch of load_game[]/stream_game[]
// (snd_utils.c:30-40,53-63) which keeps only the `.ext` half of each row when
// decode isn't linked.
//
// @thread-safety: codec instances are const/stateless singletons — safe to
// share across any number of callers/threads (S9.2: decode is not yet wired
// to T_AudioDecoder — see sound.hpp's SoundInitParams TODO — but the type is
// reentrant by construction for when it is, per P-8 annotation discipline).

#include <xash3dpp/sound/audio_data.hpp>
#include <xash3dpp/sound/errors.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace xash::sound {

// A decoder for one audio-file family (the legacy loadwavfmt_t row).
//
// @thread-safety: implementations must be stateless (no mutable members) —
// decode() is called through a `const IAudioCodec&` singleton reference and
// must be safe to invoke concurrently from any number of threads.
class IAudioCodec
{
public:
    IAudioCodec() noexcept                        = default;
    virtual ~IAudioCodec()                        = default;
    IAudioCodec( const IAudioCodec & )             = delete;
    IAudioCodec &operator=( const IAudioCodec & )  = delete;

    // True if this codec decodes files with the given lowercase extension
    // (no leading dot) — the legacy loadwavfmt_t::ext / load_game[] key.
    [[nodiscard]] virtual bool handles( std::string_view ext ) const noexcept = 0;

    // Decode a whole file buffer into an AudioData. `name` carries the
    // original filename for name-prefix quirks (the WAV allowlist's
    // null.wav/_period.wav/_comma.wav substring match — snd_wav.c:347-370).
    [[nodiscard]] virtual Result<AudioData> decode( std::string_view name,
                                                     std::span<const std::byte> file ) const = 0;

    // -----------------------------------------------------------------------
    // RESERVED (SND-OQ-4, not implemented this slice): a codec MAY also vend
    // a streaming interface for background-track playback, mirroring
    // legacy's parallel streamfmt_t v-table (soundlib.h:30-39 — open/read/
    // setpos/getpos/free, 5 fns vs. loadwavfmt_t's 1 — R9.2 evidence that
    // streaming is architecturally distinct from one-shot load already in
    // legacy). SND-OQ-4 was DECIDED 2026-07-19 (campaign B3, decisions-
    // architecture.md §3a / sound-boundary.md §Open questions): shape (a) —
    // `s_stream.c`'s background-track logic is fenced OUT of Chunk 9 scope
    // entirely, and this codec seam RESERVES the `IAudioStream` vend point
    // below so the follow-up streaming slice is purely additive rather than a
    // refactor. Uncomment once IAudioStream itself is designed:
    //
    // [[nodiscard]] virtual std::unique_ptr<IAudioStream>
    //     open_stream( std::string_view name, std::span<const std::byte> file ) const;
    // -----------------------------------------------------------------------
};

// Per-codec singleton accessors (defined in each codec_*.cpp). The registry
// below assembles from these — one line per codec as they land.
[[nodiscard]] const IAudioCodec &wav_codec() noexcept;
// [[nodiscard]] const IAudioCodec &mp3_codec() noexcept;  // reserved — Q-11 satellite target (mp3/libmpg, adjudication pending), not linked here
// [[nodiscard]] const IAudioCodec &ogg_codec() noexcept;  // reserved — Q-11 satellite target (vorbis/vorbisfile), not linked here
// [[nodiscard]] const IAudioCodec &opus_codec() noexcept; // reserved — Q-11 satellite target (opus/opusfile), not linked here

// ---------------------------------------------------------------------------
// AudioCodecEntry / k_audio_codecs — the FIXED dispatch registry (extension ->
// codec singleton). Mirrors archive_registry.hpp's k_archive_types shape: a
// constexpr array of function pointers, no dynamic registration.
// ---------------------------------------------------------------------------
struct AudioCodecEntry
{
    std::string_view extension; // lowercase, no leading dot
    const IAudioCodec &( *codec )() noexcept;
};

inline constexpr std::array<AudioCodecEntry, 1> k_audio_codecs = {{
    { "wav", &wav_codec },
    // { "mp3",  &mp3_codec },  // reserved — see mp3_codec() above
    // { "ogg",  &ogg_codec },  // reserved — see ogg_codec() above
    // { "opus", &opus_codec }, // reserved — see opus_codec() above
}};

// k_audio_extensions — extension-ONLY table (legacy XASH_DEDICATED branch,
// snd_utils.c:35-39,58-63): every soundlib format extension WITHOUT a codec
// function pointer attached, so a dedicated-server-style "is foo.mp3 a sound
// file?" query (Sound_SupportedFileFormat parity) never pulls in decode code
// for formats that are not (or not yet) linked — the whole point of keeping
// this SEPARATE from k_audio_codecs above.
inline constexpr std::array<std::string_view, 4> k_audio_extensions = {
    "wav", "mp3", "ogg", "opus",
};

// True if `ext` (lowercase, no leading dot) names a recognised soundlib
// format — dedicated-style query, does not require a linked codec (mirrors
// Sound_SupportedFileFormat, snd_utils.c:447-459, against k_audio_extensions).
[[nodiscard]] constexpr bool is_supported_audio_extension( std::string_view ext ) noexcept
{
    for( const std::string_view e : k_audio_extensions )
        if( e == ext )
            return true;
    return false;
}

// Linear scan of k_audio_codecs for a codec whose handles(ext) is true
// (mirrors ImageDecoder::decode's registry loop, src/content/imagelib/
// imagelib.cpp). Returns nullptr if no linked codec claims `ext`.
[[nodiscard]] inline const IAudioCodec *find_audio_codec( std::string_view ext ) noexcept
{
    for( const AudioCodecEntry &entry : k_audio_codecs )
    {
        const IAudioCodec &codec = entry.codec();
        if( codec.handles( ext ) )
            return &codec;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Sound_GetApproxWavePlayLen port (snd_utils.c:85-120): estimate playback
// length in milliseconds from a WAV header + the file's total size, without a
// full decode (GAME_EXPORT in legacy — a standalone fast path bypassing the
// format table entirely, snd_utils.c:314 note).
//
// Signature deviation (documented, not silent): legacy opens the file itself
// via FS_Open/FS_Read/FS_FileLength (snd_utils.c:91-105) — filesystem I/O is
// sibling-owned per the boundary's sibling-scope rule, and this slice does not
// wire a Filesystem dependency into the codec layer (SoundInitParams::filesystem
// is a TODO for S9.2/S9.3, sound.hpp). Callers supply the header bytes they
// already read (>= 36 bytes, sizeof legacy wavehdr_t) plus the total file size
// obtained however they like (e.g. Filesystem::file_size); the value math is a
// verbatim port, including the filesize-128 GoldSrc magic (snd_utils.c:102-103,
// "magic number from GoldSrc, seems to be header size").
//
// Implemented in codec_wav.cpp (WAV-header-specific parsing).
[[nodiscard]] std::uint32_t approx_wave_play_len( std::span<const std::byte> header,
                                                   std::size_t                file_size ) noexcept;

} // namespace xash::sound
