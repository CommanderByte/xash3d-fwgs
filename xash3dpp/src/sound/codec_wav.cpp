// xash3dpp — WAV audio codec (Chunk 9, slice S9.2)
// Legacy reference: engine/client/soundlib/snd_wav.c (Sound_LoadWAV :177-373,
// the FindChunk/FindNextChunk IFF walk :26-132), engine/common/soundlib/
// snd_utils.c (Sound_GetApproxWavePlayLen :85-120), soundlib.h (wavehdr_t,
// RIFFHEADER/WAVEHEADER/FORMHEADER, sndlib_t).
//
// Full snd_wav.c parity, WITHOUT the legacy file-scope `iff_*` statics
// (snd_wav.c:20-24 — the deep-dive's flagged port hazard, "Race-shared...
// same class as `sound`"): every call gets its own WavCursor on the stack, so
// WavCodec::decode is reentrant (matches the class's stateless-singleton
// contract, private/sound/codec.hpp).
//
// @thread-safety: pure function over a caller-owned buffer — no shared state,
// safe to call from any thread / concurrently (P-8: annotated here instead of
// per-call, since there is nothing to assert — the type has no mutable state
// to race on).

#include <xash3dpp/private/sound/codec.hpp>

#include <xash3dpp/utilities/hash.hpp>
#include <xash3dpp/utilities/swap.hpp>

#include <algorithm>
#include <cstring>
#include <optional>

namespace xash::sound {
namespace {

// ---------------------------------------------------------------------------
// Small bounds-checked byte helpers (H-4 discipline: every read is range-
// checked; a malformed file yields SoundError::DecodeFailed, never a fault).
// ---------------------------------------------------------------------------

[[nodiscard]] bool four_cc( std::span<const std::byte> file, std::size_t off, const char *tag ) noexcept
{
    if( off + 4 > file.size() )
        return false;
    return std::memcmp( file.data() + off, tag, 4 ) == 0;
}

// Sequential bounds-checked reader — the modern replacement for legacy's
// GetLittleShort()/GetLittleLong() (snd_wav.c:36-63), which read+advance a
// single file-scope `iff_dataPtr`. Here the cursor is a local, so multiple
// parses (or concurrent decode() calls) never share mutable state.
class ByteCursor
{
public:
    ByteCursor( std::span<const std::byte> file, std::size_t pos ) noexcept : file_( file ), pos_( pos ) {}

    [[nodiscard]] std::size_t pos() const noexcept { return pos_; }
    void seek( std::size_t p ) noexcept { pos_ = p; }
    void skip( std::size_t n ) noexcept { pos_ += n; }

    [[nodiscard]] std::optional<std::uint16_t> u16() noexcept
    {
        auto v = xash::utilities::read_le<std::uint16_t>( file_, pos_ );
        if( v )
            pos_ += 2;
        return v;
    }
    [[nodiscard]] std::optional<std::uint32_t> u32() noexcept
    {
        auto v = xash::utilities::read_le<std::uint32_t>( file_, pos_ );
        if( v )
            pos_ += 4;
        return v;
    }

private:
    std::span<const std::byte> file_;
    std::size_t                pos_;
};

// ---------------------------------------------------------------------------
// WavCursor — the local parse-cursor replacing snd_wav.c's file-scope
// iff_data/iff_dataPtr/iff_end/iff_lastChunk/iff_chunkLen. Positions are byte
// OFFSETS from the file's start (not raw pointers) so every access can be
// bounds-checked against `file.size()` instead of trusting `iff_end`.
//
// find_chunk()/find_next_chunk() below are a direct port of FindChunk /
// FindNextChunk (snd_wav.c:70-132): find_chunk() resets the scan to
// `data_base` (iff_data) and calls find_next_chunk(); find_next_chunk() walks
// forward from `last_chunk` (iff_lastChunk), matching legacy's chunk-skip /
// truncation-clamp / odd-length-pad arithmetic exactly. On success `found`
// holds the offset of the matched chunk's 4-byte NAME (matching legacy's
// iff_dataPtr post-condition — callers skip +8 for chunk data, or +4 to reach
// the length field, exactly as Sound_LoadWAV does).
// ---------------------------------------------------------------------------
struct WavCursor
{
    std::span<const std::byte> file;
    std::size_t                data_base  = 0; // iff_data
    std::size_t                last_chunk = 0; // iff_lastChunk
    std::optional<std::size_t> found;          // iff_dataPtr (name offset) or nullopt
};

void find_next_chunk( WavCursor &c, const char *name ) noexcept
{
    while( true )
    {
        if( c.last_chunk >= c.file.size() )
        {
            c.found.reset();
            return;
        }
        std::size_t remaining = c.file.size() - c.last_chunk;
        if( remaining < 8 )
        {
            c.found.reset();
            return;
        }

        // length field sits at last_chunk+4..+8 (name occupies last_chunk+0..+4)
        const auto raw = xash::utilities::read_le<std::uint32_t>( c.file, c.last_chunk + 4 );
        if( !raw )
        {
            c.found.reset();
            return;
        }
        remaining -= 8; // bytes available after the 8-byte chunk header

        // legacy iff_chunkLen is `int` (signed) — a length whose top bit is
        // set reads as negative and aborts the WHOLE search (snd_wav.c:86-90),
        // not just this one candidate chunk.
        if( ( *raw & 0x8000'0000u ) != 0 )
        {
            c.found.reset();
            return;
        }

        std::size_t chunk_len = static_cast<std::size_t>( *raw );
        if( chunk_len > remaining )
            chunk_len = remaining; // clamp to what's actually there (legacy: warn+clamp, snd_wav.c:92-110; the
                                    // warning print is intentionally dropped — behaviour parity, not diagnostics)

        remaining -= chunk_len;

        const std::size_t name_pos = c.last_chunk; // iff_dataPtr after the "-= 8" correction
        std::size_t       next     = name_pos + 8 + chunk_len;
        if( ( chunk_len & 1 ) && remaining )
            ++next;
        c.last_chunk = next;

        if( four_cc( c.file, name_pos, name ) )
        {
            c.found = name_pos;
            return;
        }
        // else: keep scanning from the updated last_chunk
    }
}

void find_chunk( WavCursor &c, const char *name ) noexcept
{
    c.last_chunk = c.data_base;
    find_next_chunk( c, name );
}

// ---------------------------------------------------------------------------
// Case-insensitive substring match (ASCII) — legacy Q_stristr, used only for
// the 3-entry broken-WAV CRC allowlist's filename gate (snd_wav.c:348).
// ---------------------------------------------------------------------------
[[nodiscard]] bool contains_ci( std::string_view haystack, std::string_view needle ) noexcept
{
    if( needle.empty() || needle.size() > haystack.size() )
        return needle.empty();
    auto lower = []( char c ) noexcept { return ( c >= 'A' && c <= 'Z' ) ? static_cast<char>( c - 'A' + 'a' ) : c; };
    for( std::size_t i = 0; i + needle.size() <= haystack.size(); ++i )
    {
        bool match = true;
        for( std::size_t j = 0; j < needle.size(); ++j )
        {
            if( lower( haystack[i + j] ) != lower( needle[j] ) )
            {
                match = false;
                break;
            }
        }
        if( match )
            return true;
    }
    return false;
}

// The hardcoded CRC32 allowlist of 3 known-broken HL1/Q1 WAVs (snd_wav.c:
// 350-355) — silently zeroed at load time. CRCs are IEEE 802.3 / zlib-variant
// (xash::utilities::crc32 — the crclib port, init 0xFFFFFFFF, final XOR
// 0xFFFFFFFF), computed over the WHOLE original file buffer (not just the
// data chunk), matching `CRC32_ProcessBuffer(&crc, buffer, filesize)`.
constexpr std::uint32_t k_broken_wav_crcs[3] = {
    0x14a36f29u, // common/null.wav (HL1/Q1)
    0x005a43abu, // vox/_period.wav (HL1)
    0x7749ed15u, // vox/_comma.wav (HL1)
};

[[nodiscard]] bool matches_broken_wav_crc( std::span<const std::byte> file ) noexcept
{
    const std::uint32_t crc = xash::utilities::crc32( file.data(), file.size() );
    for( const std::uint32_t known : k_broken_wav_crcs )
        if( crc == known )
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// decode_wav — the full Sound_LoadWAV port (snd_wav.c:177-373).
// ---------------------------------------------------------------------------
[[nodiscard]] Result<AudioData> decode_wav( std::string_view name, std::span<const std::byte> file )
{
    if( file.empty() )
        return std::unexpected( SoundError::DecodeFailed ); // legacy: !buffer || filesize<=0

    WavCursor c { file, 0, 0, std::nullopt };

    // ---- "RIFF" chunk, then verify "WAVE" at data+0..4 (snd_wav.c:188-195) ----
    find_chunk( c, "RIFF" );
    if( !c.found || !four_cc( file, *c.found + 8, "WAVE" ) )
        return std::unexpected( SoundError::DecodeFailed ); // missing 'RIFF/WAVE' chunks

    // ---- "fmt " chunk, scanning the RIFF payload (past the "WAVE" tag) ----
    c.data_base = *c.found + 12; // name(4)+len(4)+"WAVE"(4)
    find_chunk( c, "fmt " );
    if( !c.found )
        return std::unexpected( SoundError::DecodeFailed ); // missing 'fmt ' chunk

    ByteCursor fmt( file, *c.found + 8 ); // skip name(4)+len(4)
    const auto fmt_tag = fmt.u16();
    if( !fmt_tag )
        return std::unexpected( SoundError::DecodeFailed );

    // legacy accepts EXACTLY two format tags: 1 (Microsoft PCM) or 85 (MPEG
    // audio packed in a WAV container, fmt != 1 branch, snd_wav.c:210-222).
    // fmt==85 hands off to Sound_LoadMPG (snd_wav.c:300-313) — the mp3
    // satellite target (boundary Q-11) is not linked this slice, so a
    // recognised-but-unsupported mpeg-in-wav container fails decode here
    // rather than silently mis-parsing PCM fields that don't apply to it.
    if( *fmt_tag != 1 )
        return std::unexpected( SoundError::DecodeFailed ); // not a Microsoft PCM format (incl. fmt==85 mpeg-in-wav)

    const auto channels_raw = fmt.u16();
    if( !channels_raw || ( *channels_raw != 1 && *channels_raw != 2 ) )
        return std::unexpected( SoundError::DecodeFailed ); // only mono/stereo supported

    const auto rate_raw = fmt.u32();
    if( !rate_raw )
        return std::unexpected( SoundError::DecodeFailed );
    fmt.skip( 6 ); // nAvgBytesPerSec(4) + nBlockAlign(2)

    const auto bits_per_sample = fmt.u16();
    if( !bits_per_sample )
        return std::unexpected( SoundError::DecodeFailed );
    const std::uint32_t width_raw = *bits_per_sample / 8;
    if( width_raw != 1 && width_raw != 2 )
        return std::unexpected( SoundError::DecodeFailed ); // only 8/16-bit supported

    const std::uint32_t channels = *channels_raw;
    const std::uint32_t width    = width_raw;

    // ---- "cue " chunk -> loop_start (+ the LIST/"mark" length quirk) ----
    // snd_wav.c:244-267. FindChunk resets the scan to data_base (same RIFF
    // payload region as the fmt search); the LIST lookup below CONTINUES from
    // there via find_next_chunk (not find_chunk) — matching legacy exactly.
    bool          looped          = false;
    std::uint32_t loop_start_raw  = 0;
    std::optional<std::uint32_t> cue_total_samples; // legacy: sound.samples set via the "mark" sub-case only

    find_chunk( c, "cue " );
    if( c.found && file.size() - *c.found >= 36 )
    {
        const auto ls = xash::utilities::read_le<std::uint32_t>( file, *c.found + 32 );
        if( !ls )
            return std::unexpected( SoundError::DecodeFailed );
        loop_start_raw = *ls;
        looped         = true;

        find_next_chunk( c, "LIST" ); // continues scanning past the cue chunk (NOT a fresh find_chunk)
        if( c.found && file.size() - *c.found >= 32 )
        {
            if( four_cc( file, *c.found + 28, "mark" ) )
            {
                // legacy: `iff_dataPtr += 24; GetLittleLong();` — iff_dataPtr
                // is already an ABSOLUTE pointer (name_pos), so this reads at
                // name_pos+24 (payload offset 16, since payload starts at +8).
                const auto mark_len = xash::utilities::read_le<std::uint32_t>( file, *c.found + 24 );
                if( !mark_len )
                    return std::unexpected( SoundError::DecodeFailed );
                // legacy: sound.samples = sound.loopstart + GetLittleLong() — an
                // across-channel total, added to loop_start's raw (also
                // across-channel-unaware) units verbatim, quirk preserved.
                cue_total_samples = loop_start_raw + *mark_len;
            }
        }
    }

    // ---- "data" chunk: locate, derive the (possibly cue-truncated) sample
    // count, then copy + convert the PCM payload (snd_wav.c:270-345) ----
    find_chunk( c, "data" );
    if( !c.found )
        return std::unexpected( SoundError::DecodeFailed ); // missing 'data' chunk

    const std::size_t          data_len_pos = *c.found + 4; // skip name only -> length field
    const auto                 raw_data_len = xash::utilities::read_le<std::uint32_t>( file, data_len_pos );
    if( !raw_data_len )
        return std::unexpected( SoundError::DecodeFailed );
    const std::size_t data_start = data_len_pos + 4;

    // legacy: samples = GetLittleLong() / sound.width  (across-channel total)
    const std::uint32_t samples_from_data = *raw_data_len / width;

    std::uint32_t sound_samples; // legacy: sound.samples, PRE channel-divide
    // Legacy gates the cue branch on `if( sound.samples )` — a VALUE test, not
    // presence: a cue total of 0 (loopstart 0 + mark length 0) falls through to
    // the full data-chunk count and decodes successfully as a looped sound
    // (snd_wav.c:281-289). Parity-audit fix 2026-07-20.
    if( cue_total_samples.has_value() && *cue_total_samples != 0 )
    {
        if( samples_from_data < *cue_total_samples )
            return std::unexpected( SoundError::DecodeFailed ); // "bad loop length"
        sound_samples = *cue_total_samples; // NOTE: kept as the (possibly smaller)
                                             // cue-derived total, not overwritten by
                                             // samples_from_data — this is the
                                             // truncation quirk (snd_wav.c:281-289).
    }
    else
    {
        sound_samples = samples_from_data;
    }
    if( sound_samples == 0 )
        return std::unexpected( SoundError::DecodeFailed ); // "file with 0 samples"

    const std::uint32_t per_channel_samples = sound_samples / channels; // legacy: sound.samples /= sound.channels

    // Byte size to copy: computed AFTER the channel-divide (legacy order), in
    // a widened accumulator so a corrupt/oversized declared length can never
    // wrap back into a small, falsely-passing value on a 32-bit size_t build.
    const std::uint64_t copy_size64 =
        static_cast<std::uint64_t>( per_channel_samples ) * width * channels;

    // SAFETY GUARD (documented deviation, not a silent behaviour change): a
    // "data" chunk MAY declare more bytes than the file actually contains
    // (truncated download, corrupt file, or an over-large declared length
    // that survives the FindNextChunk truncation clamp because Sound_LoadWAV
    // re-reads the raw length field directly, snd_wav.c:279 vs. the clamped
    // iff_chunkLen used only for chunk-skip bookkeeping). Legacy trusts this
    // value for both Mem_Malloc's size and the subsequent memcpy, which is an
    // out-of-bounds read for such a file. This port refuses instead (H-4
    // discipline: bounds-checked reads, never a fault) — the copy size and
    // sample-count MATH above is bit-exact with legacy for every well-formed
    // file; only genuinely truncated/corrupt "data" chunks take this path.
    if( copy_size64 > file.size() - data_start )
        return std::unexpected( SoundError::DecodeFailed );

    const std::size_t copy_size = static_cast<std::size_t>( copy_size64 );

    std::vector<std::byte> out( copy_size );
    if( width == 2 )
    {
        // "swap 16-bit samples from little endian to native" (snd_wav.c:322-329):
        // read_le already performs the byteswap-if-big-endian; storing the
        // resulting int16_t's native bit pattern reproduces the in-memory
        // result exactly on every host.
        const std::size_t count = copy_size / 2;
        for( std::size_t i = 0; i < count; ++i )
        {
            const std::int16_t sample =
                xash::utilities::read_le<std::int16_t>( file.data() + data_start + i * 2 );
            std::memcpy( out.data() + i * 2, &sample, sizeof( sample ) );
        }
    }
    else // width == 1
    {
        // "now convert 8-bit sounds to signed" (snd_wav.c:331-345): WAV 8-bit
        // PCM is unsigned; legacy stores it SIGNED in wavdata_t (subtract 128,
        // wrapping). Bit-identical to XORing the high bit, done here as the
        // literal subtraction to keep the port line-for-line traceable.
        for( std::size_t i = 0; i < copy_size; ++i )
        {
            const std::uint8_t raw8    = std::to_integer<std::uint8_t>( file[data_start + i] );
            const std::uint8_t signed8 = static_cast<std::uint8_t>( static_cast<int>( raw8 ) - 128 );
            out[i] = std::byte{ signed8 };
        }
    }

    // ---- known-broken-WAV CRC32 allowlist (snd_wav.c:347-370) ----
    if( contains_ci( name, "null.wav" ) || contains_ci( name, "_period.wav" ) || contains_ci( name, "_comma.wav" ) )
    {
        if( matches_broken_wav_crc( file ) )
            std::fill( out.begin(), out.end(), std::byte{ 0 } );
    }

    AudioData result;
    result.rate       = *rate_raw;
    result.width      = static_cast<std::uint8_t>( width );
    result.channels   = static_cast<std::uint8_t>( channels );
    result.loop_start = looped ? loop_start_raw : 0;
    result.samples    = per_channel_samples;
    result.type       = AudioFormatType::Pcm;
    result.flags      = looped ? AudioFlags::Looped : AudioFlags::None;
    result.buffer     = std::move( out );
    return result;
}

// ---- the registered codec ---------------------------------------------
class WavCodec final : public IAudioCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "wav"; }
    [[nodiscard]] Result<AudioData> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_wav( name, file );
    }
};

} // namespace

const IAudioCodec &wav_codec() noexcept
{
    static const WavCodec codec;
    return codec;
}

// ---------------------------------------------------------------------------
// Sound_GetApproxWavePlayLen port (snd_utils.c:85-120). See codec.hpp for the
// signature-deviation rationale (header bytes + file_size, not a filename).
// ---------------------------------------------------------------------------
std::uint32_t approx_wave_play_len( std::span<const std::byte> header, std::size_t file_size ) noexcept
{
    // sizeof(wavehdr_t) (soundlib.h:93-106): riff_id/rLen/wave_id/fmt_id/
    // pcm_header_len (5*int32=20) + wFormatTag/nChannels (2*int16=4) +
    // nSamplesPerSec (int32=4) + nAvgBytesPerSec (int32=4) + nBlockAlign/
    // nBitsPerSample (2*int16=4) == 36 bytes.
    constexpr std::size_t k_wavehdr_size = 36;
    // "magic number from GoldSrc, seems to be header size" (snd_utils.c:102-103).
    constexpr std::size_t k_goldsrc_magic = 128;

    if( header.size() < k_wavehdr_size )
        return 0;

    if( !four_cc( header, 0, "RIFF" ) || !four_cc( header, 8, "WAVE" ) || !four_cc( header, 12, "fmt " ) )
        return 0; // legacy: wav.riff_id/wave_id/fmt_id magic check

    // DEVIATION (documented, not silent): legacy computes
    // `size_t filesize = FS_FileLength(f) - 128;` with NO underflow guard —
    // on a file shorter than 128 bytes this wraps to a huge value, and the
    // later float->uint cast on an out-of-range value is undefined behaviour
    // in C. There is no meaningful "play length" for a file this short
    // anyway, so this port returns 0 instead of reproducing the UB.
    if( file_size < k_goldsrc_magic )
        return 0;
    const std::size_t filesize = file_size - k_goldsrc_magic;

    const auto avg_bytes_raw = xash::utilities::read_le<std::int32_t>( header, 28 );
    if( !avg_bytes_raw )
        return 0;
    const std::int32_t avg_bytes = *avg_bytes_raw;
    // legacy: an avgBytes <= 0 divides by zero/negative in float (well-defined
    // as +-Inf) then casts that Inf to `uint`, which IS undefined behaviour in
    // C. Same reasoning as above: refuse rather than reproduce the UB.
    if( avg_bytes <= 0 )
        return 0;

    // legacy arithmetic verbatim, in `float` to match its rounding exactly
    // (snd_utils.c:111-117).
    float msecs;
    if( avg_bytes >= 1000 )
        msecs = static_cast<float>( filesize ) / ( static_cast<float>( avg_bytes ) / 1000.0f );
    else
        msecs = ( static_cast<float>( filesize ) / static_cast<float>( avg_bytes ) ) * 1000.0f;

    return static_cast<std::uint32_t>( msecs );
}

} // namespace xash::sound
