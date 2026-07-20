// xash3dpp — sound codec tests (Chunk 9, slice S9.2)
// Covers: IAudioCodec/WavCodec parity against engine/client/soundlib/snd_wav.c
// + engine/common/soundlib/snd_utils.c's Sound_GetApproxWavePlayLen.  All WAV
// images are synthesized in-test (tests/README no-committed-binaries rule) —
// no fixture files.

#include <xash3dpp/private/sound/codec.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::sound;

// ---------------------------------------------------------------------------
// WAV synthesis helpers — hand-rolled RIFF/WAVE chunk builder.
// ---------------------------------------------------------------------------
namespace {

void put_u8( std::vector<std::byte> &v, std::uint8_t x ) { v.push_back( std::byte{ x } ); }

void put_u16le( std::vector<std::byte> &v, std::uint16_t x )
{
    put_u8( v, static_cast<std::uint8_t>( x & 0xFF ) );
    put_u8( v, static_cast<std::uint8_t>( ( x >> 8 ) & 0xFF ) );
}

void put_i16le( std::vector<std::byte> &v, std::int16_t x ) { put_u16le( v, static_cast<std::uint16_t>( x ) ); }

void put_u32le( std::vector<std::byte> &v, std::uint32_t x )
{
    put_u8( v, static_cast<std::uint8_t>( x & 0xFF ) );
    put_u8( v, static_cast<std::uint8_t>( ( x >> 8 ) & 0xFF ) );
    put_u8( v, static_cast<std::uint8_t>( ( x >> 16 ) & 0xFF ) );
    put_u8( v, static_cast<std::uint8_t>( ( x >> 24 ) & 0xFF ) );
}

void put_tag( std::vector<std::byte> &v, const char *tag4 )
{
    for( int i = 0; i < 4; ++i )
        put_u8( v, static_cast<std::uint8_t>( tag4[i] ) );
}

// name(4) + len(4) + payload [+ pad byte if len is odd] — the generic IFF
// chunk wrapper (snd_wav.c's FindNextChunk consumer side).
std::vector<std::byte> make_chunk( const char *tag4, const std::vector<std::byte> &payload )
{
    std::vector<std::byte> c;
    put_tag( c, tag4 );
    put_u32le( c, static_cast<std::uint32_t>( payload.size() ) );
    c.insert( c.end(), payload.begin(), payload.end() );
    if( payload.size() & 1 )
        put_u8( c, 0 );
    return c;
}

// Standard 16-byte PCM fmt payload (wFormatTag/nChannels/nSamplesPerSec/
// nAvgBytesPerSec/nBlockAlign/nBitsPerSample). format_tag defaults to 1 (PCM);
// callers pass 85 (mpeg-in-wav) or an invalid tag to exercise rejection paths.
std::vector<std::byte> make_fmt_chunk( std::uint16_t channels, std::uint32_t rate, std::uint16_t bits_per_sample,
                                       std::uint16_t format_tag = 1 )
{
    std::vector<std::byte> p;
    put_u16le( p, format_tag );
    put_u16le( p, channels );
    put_u32le( p, rate );
    const std::uint32_t block_align = static_cast<std::uint32_t>( channels ) * ( bits_per_sample / 8u );
    put_u32le( p, rate * block_align );          // nAvgBytesPerSec (unused by decode)
    put_u16le( p, static_cast<std::uint16_t>( block_align ) );
    put_u16le( p, bits_per_sample );
    return make_chunk( "fmt ", p );
}

std::vector<std::byte> make_data_chunk( const std::vector<std::byte> &pcm ) { return make_chunk( "data", pcm ); }

// A minimal "cue " chunk: dwCuePoints=1 + one 24-byte cue-point record whose
// dwSampleOffset == loop_start (snd_wav.c:244-262 field layout).
std::vector<std::byte> make_cue_chunk( std::uint32_t loop_start )
{
    std::vector<std::byte> p;
    put_u32le( p, 1 );        // dwCuePoints
    put_u32le( p, 0 );        // dwName
    put_u32le( p, 0 );        // dwPosition
    put_tag( p, "data" );     // fccChunk
    put_u32le( p, 0 );        // dwChunkStart
    put_u32le( p, 0 );        // dwBlockStart
    put_u32le( p, loop_start ); // dwSampleOffset
    return make_chunk( "cue ", p ); // 28-byte payload (even, no pad)
}

// A "LIST" chunk carrying the CoolEdit "mark" sub-blob at payload offset 16-23
// (snd_wav.c:255-260: "mark" fourcc at name_pos+28 == payload offset 20).
std::vector<std::byte> make_list_mark_chunk( std::uint32_t mark_len )
{
    std::vector<std::byte> p;
    for( int i = 0; i < 16; ++i )
        put_u8( p, 0 );          // filler up to payload offset 16
    put_u32le( p, mark_len );    // payload[16..19]
    put_tag( p, "mark" );        // payload[20..23]
    return make_chunk( "LIST", p ); // 24-byte payload (even, no pad)
}

std::vector<std::byte> make_wav( const std::vector<std::byte> &fmt_chunk, const std::vector<std::byte> &data_chunk,
                                 const std::vector<std::byte> &cue_chunk  = {},
                                 const std::vector<std::byte> &list_chunk = {} )
{
    std::vector<std::byte> riff_payload;
    put_tag( riff_payload, "WAVE" );
    riff_payload.insert( riff_payload.end(), fmt_chunk.begin(), fmt_chunk.end() );
    riff_payload.insert( riff_payload.end(), cue_chunk.begin(), cue_chunk.end() );
    riff_payload.insert( riff_payload.end(), list_chunk.begin(), list_chunk.end() );
    riff_payload.insert( riff_payload.end(), data_chunk.begin(), data_chunk.end() );

    std::vector<std::byte> out;
    put_tag( out, "RIFF" );
    put_u32le( out, static_cast<std::uint32_t>( riff_payload.size() ) );
    out.insert( out.end(), riff_payload.begin(), riff_payload.end() );
    return out;
}

[[nodiscard]] std::span<const std::byte> as_span( const std::vector<std::byte> &v ) noexcept { return v; }

} // namespace

// ---------------------------------------------------------------------------
// Registry dispatch + extension array
// ---------------------------------------------------------------------------

static void test_registry_dispatch()
{
    CHECK( wav_codec().handles( "wav" ) );
    CHECK( !wav_codec().handles( "mp3" ) );
    // Dispatch keys are lowercase, no leading dot — case-insensitivity is the
    // CALLER's job (matches ImageDecoder::decode's lowercasing before lookup);
    // the codec's own handles() is an exact match.
    CHECK( !wav_codec().handles( "WAV" ) );
    CHECK( !wav_codec().handles( ".wav" ) );

    CHECK_EQ( k_audio_codecs.size(), std::size_t{ 1 } );
    CHECK( k_audio_codecs[0].extension == "wav" );

    CHECK_EQ( find_audio_codec( "wav" ), &wav_codec() );
    CHECK( find_audio_codec( "mp3" ) == nullptr );  // recognised extension, but no linked codec (reserved row)
    CHECK( find_audio_codec( "xyz" ) == nullptr );
}

static void test_extension_array()
{
    CHECK_EQ( k_audio_extensions.size(), std::size_t{ 4 } );
    CHECK( is_supported_audio_extension( "wav" ) );
    // Reserved formats are still recognised as sound extensions (dedicated-
    // style query) even though no codec is linked for them this slice.
    CHECK( is_supported_audio_extension( "mp3" ) );
    CHECK( is_supported_audio_extension( "ogg" ) );
    CHECK( is_supported_audio_extension( "opus" ) );
    CHECK( !is_supported_audio_extension( "xyz" ) );
    CHECK( !is_supported_audio_extension( "" ) );
}

// ---------------------------------------------------------------------------
// Golden decodes
// ---------------------------------------------------------------------------

static void test_decode_16bit_stereo_golden()
{
    // 4 stereo frames, hand-picked values.
    const std::int16_t samples[8] = { 100, -100, 200, -200, 300, -300, 400, -400 };
    std::vector<std::byte> pcm;
    for( const std::int16_t s : samples )
        put_i16le( pcm, s );
    REQUIRE( pcm.size() == 16 );

    const auto wav = make_wav( make_fmt_chunk( 2, 22050, 16 ), make_data_chunk( pcm ) );

    const auto result = wav_codec().decode( "test16.wav", as_span( wav ) );
    REQUIRE( result.has_value() );

    CHECK_EQ( result->rate, std::uint32_t{ 22050 } );
    CHECK_EQ( static_cast<int>( result->width ), 2 );
    CHECK_EQ( static_cast<int>( result->channels ), 2 );
    CHECK_EQ( result->loop_start, std::uint32_t{ 0 } );
    CHECK_EQ( result->samples, std::uint32_t{ 4 } );      // per-channel frame count
    CHECK( result->type == AudioFormatType::Pcm );
    CHECK( result->flags == AudioFlags::None );
    CHECK( !has_flag( result->flags, AudioFlags::Looped ) );

    // Byte-exact payload pin: on a little-endian host, 16-bit LE-round-trip
    // reproduces the exact input bytes (snd_wav.c:322-329's "swap to native").
    REQUIRE( result->buffer.size() == pcm.size() );
    CHECK( result->buffer == pcm );
}

static void test_decode_8bit_mono_signed_conversion()
{
    // Raw on-disk 8-bit PCM is UNSIGNED (WAV spec); legacy converts to SIGNED
    // at load time (snd_wav.c:331-345: `*p = (byte)((int)(byte)*p - 128)`).
    const std::vector<std::byte> pcm_unsigned = {
        std::byte{ 0 }, std::byte{ 64 }, std::byte{ 128 }, std::byte{ 192 }, std::byte{ 255 },
    };
    const auto wav = make_wav( make_fmt_chunk( 1, 8000, 8 ), make_data_chunk( pcm_unsigned ) );

    const auto result = wav_codec().decode( "test8.wav", as_span( wav ) );
    REQUIRE( result.has_value() );

    CHECK_EQ( result->rate, std::uint32_t{ 8000 } );
    CHECK_EQ( static_cast<int>( result->width ), 1 );
    CHECK_EQ( static_cast<int>( result->channels ), 1 );
    CHECK_EQ( result->samples, std::uint32_t{ 5 } );

    // 0-128=-128(0x80), 64-128=-64(0xC0), 128-128=0(0x00), 192-128=64(0x40),
    // 255-128=127(0x7F) — the unsigned->signed conversion, byte-exact.
    const std::vector<std::byte> expected = {
        std::byte{ 0x80 }, std::byte{ 0xC0 }, std::byte{ 0x00 }, std::byte{ 0x40 }, std::byte{ 0x7F },
    };
    REQUIRE( result->buffer.size() == expected.size() );
    CHECK( result->buffer == expected );
}

// ---------------------------------------------------------------------------
// cue -> loop_start semantics
// ---------------------------------------------------------------------------

static void test_decode_cue_loop_no_truncation()
{
    // 50 mono 16-bit samples: value[i] = i*10.
    std::vector<std::byte> pcm;
    for( int i = 0; i < 50; ++i )
        put_i16le( pcm, static_cast<std::int16_t>( i * 10 ) );

    const auto wav = make_wav( make_fmt_chunk( 1, 44100, 16 ), make_data_chunk( pcm ), make_cue_chunk( 10 ) );

    const auto result = wav_codec().decode( "loop.wav", as_span( wav ) );
    REQUIRE( result.has_value() );

    CHECK( has_flag( result->flags, AudioFlags::Looped ) );
    CHECK_EQ( result->loop_start, std::uint32_t{ 10 } );
    // No LIST/"mark" sub-chunk -> no truncation; the full data chunk is used.
    CHECK_EQ( result->samples, std::uint32_t{ 50 } );
    REQUIRE( result->buffer.size() == pcm.size() );
    CHECK( result->buffer == pcm );
}

static void test_decode_cue_loop_with_truncation()
{
    // Same 50-sample source; a LIST/"mark" sub-chunk sets an explicit loop
    // LENGTH (loop_start=5, mark_len=20 -> cue total=25), which legacy uses
    // to TRUNCATE the loaded sound to fewer samples than the data chunk
    // physically holds (snd_wav.c:281-289's truncation quirk).
    std::vector<std::byte> pcm;
    for( int i = 0; i < 50; ++i )
        put_i16le( pcm, static_cast<std::int16_t>( i * 10 ) );

    const auto wav = make_wav( make_fmt_chunk( 1, 44100, 16 ), make_data_chunk( pcm ), make_cue_chunk( 5 ),
                               make_list_mark_chunk( 20 ) );

    const auto result = wav_codec().decode( "loop_trunc.wav", as_span( wav ) );
    REQUIRE( result.has_value() );

    CHECK( has_flag( result->flags, AudioFlags::Looped ) );
    CHECK_EQ( result->loop_start, std::uint32_t{ 5 } );
    CHECK_EQ( result->samples, std::uint32_t{ 25 } ); // 5 + 20, truncated from 50

    // Buffer holds only the FIRST 25 samples (50 bytes) of the source.
    REQUIRE( result->buffer.size() == std::size_t{ 50 } );
    const std::vector<std::byte> expected_prefix( pcm.begin(), pcm.begin() + 50 );
    CHECK( result->buffer == expected_prefix );
}

static void test_decode_cue_zero_total_falls_through()
{
    // loopstart 0 + mark length 0 -> cue total 0.  Legacy's gate is
    // `if( sound.samples )` — a VALUE test — so a zero cue total falls
    // through to the full data-chunk count and decodes SUCCESSFULLY as a
    // looped sound (snd_wav.c:281-289).  Parity-audit regression pin
    // 2026-07-20 (the presence-test port rejected this input).
    std::vector<std::byte> pcm;
    for( int i = 0; i < 50; ++i )
        put_i16le( pcm, static_cast<std::int16_t>( i * 3 ) );

    const auto wav = make_wav( make_fmt_chunk( 1, 44100, 16 ), make_data_chunk( pcm ), make_cue_chunk( 0 ),
                               make_list_mark_chunk( 0 ) );

    const auto result = wav_codec().decode( "loop_zero.wav", as_span( wav ) );
    REQUIRE( result.has_value() );
    CHECK( has_flag( result->flags, AudioFlags::Looped ) );
    CHECK_EQ( result->loop_start, std::uint32_t{ 0 } );
    CHECK_EQ( result->samples, std::uint32_t{ 50 } ); // full data count (mono: 100 bytes / width 2)
    CHECK_EQ( result->buffer.size(), std::size_t{ 100 } ); // untruncated
}

static void test_decode_cue_loop_bad_length()
{
    // cue total (40+40=80) exceeds the data chunk's actual sample count (50)
    // -> "bad loop length" (snd_wav.c:283-288).
    std::vector<std::byte> pcm;
    for( int i = 0; i < 50; ++i )
        put_i16le( pcm, static_cast<std::int16_t>( i ) );

    const auto wav = make_wav( make_fmt_chunk( 1, 44100, 16 ), make_data_chunk( pcm ), make_cue_chunk( 40 ),
                               make_list_mark_chunk( 40 ) );

    const auto result = wav_codec().decode( "loop_bad.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

// ---------------------------------------------------------------------------
// Truncated / corrupt inputs -> SoundError::DecodeFailed
// ---------------------------------------------------------------------------

static void test_decode_empty_file()
{
    const std::vector<std::byte> empty;
    const auto result = wav_codec().decode( "empty.wav", as_span( empty ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_missing_riff()
{
    std::vector<std::byte> junk = { std::byte{ 'X' }, std::byte{ 'Y' }, std::byte{ 'Z' }, std::byte{ 'Q' } };
    const auto result = wav_codec().decode( "junk.wav", as_span( junk ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_bad_wave_tag()
{
    std::vector<std::byte> pcm( 8, std::byte{ 0 } );
    auto wav = make_wav( make_fmt_chunk( 1, 8000, 16 ), make_data_chunk( pcm ) );
    // Corrupt the "WAVE" fourcc (offset 8) to something else.
    wav[8] = std::byte{ 'X' };
    const auto result = wav_codec().decode( "badwave.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_missing_fmt()
{
    std::vector<std::byte> pcm( 8, std::byte{ 0 } );
    // RIFF/WAVE + a data chunk, but no "fmt " chunk.
    std::vector<std::byte> riff_payload;
    put_tag( riff_payload, "WAVE" );
    const auto data_chunk = make_data_chunk( pcm );
    riff_payload.insert( riff_payload.end(), data_chunk.begin(), data_chunk.end() );
    std::vector<std::byte> wav;
    put_tag( wav, "RIFF" );
    put_u32le( wav, static_cast<std::uint32_t>( riff_payload.size() ) );
    wav.insert( wav.end(), riff_payload.begin(), riff_payload.end() );

    const auto result = wav_codec().decode( "nofmt.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_bad_fmt_tag()
{
    std::vector<std::byte> pcm( 8, std::byte{ 0 } );
    // format tag 2 is neither PCM(1) nor mpeg-in-wav(85) -> rejected.
    const auto wav = make_wav( make_fmt_chunk( 1, 8000, 16, /*format_tag=*/2 ), make_data_chunk( pcm ) );
    const auto result = wav_codec().decode( "badfmt.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_mpeg_in_wav_deferred()
{
    // format tag 85 (mpeg-in-wav) is a RECOGNISED legacy format that hands off
    // to Sound_LoadMPG (snd_wav.c:300-313) — the mp3 satellite target is not
    // linked this slice, so this is a documented "valid but unsupported here"
    // rejection, not a "malformed file" one (same SoundError either way, per
    // S9.1's single content-parse error code).
    std::vector<std::byte> pcm( 8, std::byte{ 0 } );
    const auto wav = make_wav( make_fmt_chunk( 1, 8000, 16, /*format_tag=*/85 ), make_data_chunk( pcm ) );
    const auto result = wav_codec().decode( "mpeg.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_bad_channels()
{
    std::vector<std::byte> pcm( 12, std::byte{ 0 } );
    const auto wav = make_wav( make_fmt_chunk( 3, 8000, 16 ), make_data_chunk( pcm ) ); // 3 channels: invalid
    const auto result = wav_codec().decode( "bad3ch.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_bad_bit_depth()
{
    std::vector<std::byte> pcm( 12, std::byte{ 0 } );
    const auto wav = make_wav( make_fmt_chunk( 1, 8000, 24 ), make_data_chunk( pcm ) ); // 24-bit: invalid
    const auto result = wav_codec().decode( "bad24.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_missing_data()
{
    // RIFF/WAVE + fmt, but no data chunk at all.
    std::vector<std::byte> riff_payload;
    put_tag( riff_payload, "WAVE" );
    const auto fmt_chunk = make_fmt_chunk( 1, 8000, 16 );
    riff_payload.insert( riff_payload.end(), fmt_chunk.begin(), fmt_chunk.end() );
    std::vector<std::byte> wav;
    put_tag( wav, "RIFF" );
    put_u32le( wav, static_cast<std::uint32_t>( riff_payload.size() ) );
    wav.insert( wav.end(), riff_payload.begin(), riff_payload.end() );

    const auto result = wav_codec().decode( "nodata.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_zero_samples()
{
    const std::vector<std::byte> empty_pcm;
    const auto wav = make_wav( make_fmt_chunk( 1, 8000, 16 ), make_data_chunk( empty_pcm ) );
    const auto result = wav_codec().decode( "zero.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

static void test_decode_truncated_data_chunk()
{
    // A well-formed 100-byte mono/16-bit data chunk (50 samples), then the
    // trailing 20 bytes of the FILE are chopped off without correcting the
    // data chunk's declared length -- the declared byte count now exceeds
    // what is actually present (H-4 safety guard, codec_wav.cpp).
    std::vector<std::byte> pcm( 100, std::byte{ 0x11 } );
    auto wav = make_wav( make_fmt_chunk( 1, 8000, 16 ), make_data_chunk( pcm ) );
    REQUIRE( wav.size() > 20 );
    wav.resize( wav.size() - 20 );

    const auto result = wav_codec().decode( "trunc.wav", as_span( wav ) );
    REQUIRE( !result.has_value() );
    CHECK( result.error() == SoundError::DecodeFailed );
}

// ---------------------------------------------------------------------------
// Broken-WAV CRC32 allowlist (snd_wav.c:347-370)
//
// The 3 allowlisted CRCs (0x14a36f29 common/null.wav, 0x005a43ab
// vox/_period.wav, 0x7749ed15 vox/_comma.wav) are checksums of specific,
// real HL1/Q1 asset files that are not committed to this repo (tests/README's
// no-committed-binaries rule) and are not practical to synthesize a byte-exact
// preimage for (finding bytes that hash to an arbitrary fixed CRC32 while also
// parsing as a valid WAV is out of scope for this slice's test coverage).
// Per the task brief, this is a DOCUMENTED SKIP of the positive (zeroed) case.
// What IS tested here is the negative path: a filename that matches one of
// the three substring gates but whose content does NOT match the allowlisted
// CRC must be left untouched -- proving the gate is CRC-based, not name-only.
// ---------------------------------------------------------------------------

static void test_broken_wav_allowlist_name_gate_without_crc_match()
{
    std::vector<std::byte> pcm;
    for( int i = 0; i < 8; ++i )
        put_i16le( pcm, static_cast<std::int16_t>( 1000 + i ) ); // deliberately non-zero
    const auto wav = make_wav( make_fmt_chunk( 1, 8000, 16 ), make_data_chunk( pcm ) );

    // Filename matches the "null.wav" substring gate, but this synthesized
    // buffer's CRC32 will not equal 0x14a36f29 -- data must survive intact.
    const auto result = wav_codec().decode( "sound/common/null.wav", as_span( wav ) );
    REQUIRE( result.has_value() );
    CHECK( result->buffer == pcm ); // NOT zeroed
}

// ---------------------------------------------------------------------------
// Sound_GetApproxWavePlayLen (snd_utils.c:85-120)
// ---------------------------------------------------------------------------

namespace {

// A minimal 36-byte wavehdr_t-shaped header: RIFF/WAVE/fmt magic + a chosen
// nAvgBytesPerSec at offset 28. Other fields are left zero (unused by the
// function under test).
std::vector<std::byte> make_wavehdr( std::int32_t avg_bytes_per_sec )
{
    std::vector<std::byte> h;
    put_tag( h, "RIFF" );
    put_u32le( h, 0 );             // rLen (unused)
    put_tag( h, "WAVE" );
    put_tag( h, "fmt " );
    put_u32le( h, 16 );            // pcm_header_len (unused)
    put_u16le( h, 1 );             // wFormatTag (unused)
    put_u16le( h, 2 );             // nChannels (unused)
    put_u32le( h, 44100 );         // nSamplesPerSec (unused)
    put_u32le( h, static_cast<std::uint32_t>( avg_bytes_per_sec ) ); // offset 28
    put_u16le( h, 4 );             // nBlockAlign (unused)
    put_u16le( h, 16 );            // nBitsPerSample (unused)
    REQUIRE( h.size() == 36 );
    return h;
}

} // namespace

static void test_approx_wave_play_len_ge_1000_branch()
{
    const auto header = make_wavehdr( 1000 );
    // file_size - 128(magic) == 1000; avg>=1000 branch: 1000/(1000/1000)=1000
    CHECK_EQ( approx_wave_play_len( as_span( header ), 1128 ), std::uint32_t{ 1000 } );
}

static void test_approx_wave_play_len_lt_1000_branch()
{
    const auto header = make_wavehdr( 500 );
    // filesize = 378-128 = 250; avg<1000 branch: (250/500)*1000 = 500
    CHECK_EQ( approx_wave_play_len( as_span( header ), 378 ), std::uint32_t{ 500 } );
}

static void test_approx_wave_play_len_short_header()
{
    std::vector<std::byte> short_header( 20, std::byte{ 0 } );
    CHECK_EQ( approx_wave_play_len( as_span( short_header ), 1000 ), std::uint32_t{ 0 } );
}

static void test_approx_wave_play_len_bad_magic()
{
    auto header = make_wavehdr( 1000 );
    header[0] = std::byte{ 'X' }; // corrupt "RIFF"
    CHECK_EQ( approx_wave_play_len( as_span( header ), 1128 ), std::uint32_t{ 0 } );
}

static void test_approx_wave_play_len_file_size_under_128()
{
    // Documented deviation: legacy underflows `size_t filesize = FS_FileLength
    // - 128` here (UB downstream); this port refuses instead of reproducing
    // the UB (see approx_wave_play_len's comment, codec_wav.cpp).
    const auto header = make_wavehdr( 1000 );
    CHECK_EQ( approx_wave_play_len( as_span( header ), 50 ), std::uint32_t{ 0 } );
}

static void test_approx_wave_play_len_zero_avg_bytes()
{
    const auto header = make_wavehdr( 0 );
    CHECK_EQ( approx_wave_play_len( as_span( header ), 1128 ), std::uint32_t{ 0 } );
}

int main()
{
    RUN_TEST( test_registry_dispatch );
    RUN_TEST( test_extension_array );

    RUN_TEST( test_decode_16bit_stereo_golden );
    RUN_TEST( test_decode_8bit_mono_signed_conversion );

    RUN_TEST( test_decode_cue_loop_no_truncation );
    RUN_TEST( test_decode_cue_loop_with_truncation );
    RUN_TEST( test_decode_cue_zero_total_falls_through );
    RUN_TEST( test_decode_cue_loop_bad_length );

    RUN_TEST( test_decode_empty_file );
    RUN_TEST( test_decode_missing_riff );
    RUN_TEST( test_decode_bad_wave_tag );
    RUN_TEST( test_decode_missing_fmt );
    RUN_TEST( test_decode_bad_fmt_tag );
    RUN_TEST( test_decode_mpeg_in_wav_deferred );
    RUN_TEST( test_decode_bad_channels );
    RUN_TEST( test_decode_bad_bit_depth );
    RUN_TEST( test_decode_missing_data );
    RUN_TEST( test_decode_zero_samples );
    RUN_TEST( test_decode_truncated_data_chunk );

    RUN_TEST( test_broken_wav_allowlist_name_gate_without_crc_match );

    RUN_TEST( test_approx_wave_play_len_ge_1000_branch );
    RUN_TEST( test_approx_wave_play_len_lt_1000_branch );
    RUN_TEST( test_approx_wave_play_len_short_header );
    RUN_TEST( test_approx_wave_play_len_bad_magic );
    RUN_TEST( test_approx_wave_play_len_file_size_under_128 );
    RUN_TEST( test_approx_wave_play_len_zero_avg_bytes );

    std::printf( "sound_codec: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
