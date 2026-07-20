// xash3dpp — VOX sentence word-sequencer tests (Chunk 9, slice S9.4).
// PARITY-CRITICAL. Section 1 ports every engine/client/sound/s_vox.c
// XASH_ENGINE_TESTS case verbatim (s_vox.c:600-729, Test_RunVOX). Later
// sections hand-derive additional coverage per the S9.4 brief: param-grammar
// edge cases, the FreeWord unconditional-zeroing quirk, TrimStart/End
// vectors (including the stereo stride quirk), the ModifyPitch float-chain
// pin composed with compute_channel_pitch, the single-immediate-slot
// overwrite quirk, sentence-terminates-on-bad-word, and a full 3-word
// sentence mix through vox_mix_channel_to_buffer (byte-pinned).

#include <xash3dpp/private/sound/vox.hpp>
#include <xash3dpp/private/sound/mixer.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/sound/audio_data.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::sound;
using ::xash::abi::portable_samplepair_t;

// ---------------------------------------------------------------------------
// AudioData builders (mirrors tests/sound/test_sound_mixer.cpp's helpers).
// ---------------------------------------------------------------------------
namespace {

AudioData make_mono8( std::initializer_list<int> samples ) noexcept
{
    AudioData a;
    a.rate     = 44100;
    a.width    = 1;
    a.channels = 1;
    a.samples  = static_cast<std::uint32_t>( samples.size() );
    a.type     = AudioFormatType::Pcm;
    a.flags    = AudioFlags::None;
    a.buffer.resize( samples.size() );
    std::size_t i = 0;
    for( int s : samples )
        a.buffer[i++] = std::byte{ static_cast<std::uint8_t>( static_cast<std::int8_t>( s ) ) };
    return a;
}

AudioData make_stereo8( std::initializer_list<int> interleaved, std::uint32_t frames ) noexcept
{
    AudioData a;
    a.rate     = 44100;
    a.width    = 1;
    a.channels = 2;
    a.samples  = frames;
    a.type     = AudioFormatType::Pcm;
    a.flags    = AudioFlags::None;
    a.buffer.resize( interleaved.size() );
    std::size_t i = 0;
    for( int s : interleaved )
        a.buffer[i++] = std::byte{ static_cast<std::uint8_t>( static_cast<std::int8_t>( s ) ) };
    return a;
}

std::span<const std::byte> as_span( const std::string &s ) noexcept
{
    return std::span<const std::byte>( reinterpret_cast<const std::byte *>( s.data() ), s.size() );
}

} // namespace

// ===========================================================================
// 1. Ported XASH_ENGINE_TESTS cases (s_vox.c:600-729).
// ===========================================================================

// s_vox.c:603-622 Test_VOX_GetDirectory — 4 table-driven cases.
static void test_ported_vox_get_directory()
{
    struct Case { std::string_view in, want_rest, want_dir; };
    const Case cases[] = {
        { "", "", "vox/" },
        { "bark bark", "bark bark", "vox/" },
        { "barney/meow", "meow", "barney/" },
        { "/fvox/_period", "_period", "fvox/" },
    };
    for( const Case &c : cases )
    {
        std::string dir;
        auto        rest = vox_get_directory( c.in, dir );
        REQUIRE( rest.has_value() );
        CHECK( *rest == c.want_rest );
        CHECK( dir == c.want_dir );
    }
}

// s_vox.c:624-656 Test_VOX_LookupString — 9 cases against 5 manually-seeded
// entries (bypassing file parsing, exactly like the legacy test).
static void test_ported_vox_lookup_string()
{
    const std::vector<VoxSentenceEntry> table = {
        { "exactmatch", "123" },
        { "CaseInsensitive", "456" },
        { "SentenceWithTabs", "\t\t\t789" },
        { "SentenceWithSpaces", "  SPAAACE" },
        { "SentenceWithTabsAndSpaces", "\t \t\t MEOW" },
    };

    struct Case { std::string_view in; const char *want; }; // want == nullptr -> expect nullopt
    const Case cases[] = {
        { "0", "123" },
        { "3", "SPAAACE" },
        { "-2", nullptr },
        { "404", nullptr },
        { "not found", nullptr },
        { "exactmatch", "123" },
        { "caseinsensitive", "456" },
        { "SentenceWithTabs", "789" },
        { "SentenceWithSpaces", "SPAAACE" },
    };

    for( const Case &c : cases )
    {
        auto got = vox_lookup_string( table, c.in );
        if( c.want == nullptr )
        {
            CHECK( !got.has_value() );
        }
        else
        {
            REQUIRE( got.has_value() );
            CHECK( *got == std::string_view( c.want ) );
        }
    }
}

// s_vox.c:658-690 Test_VOX_ParseString — 2 sentences pin punctuation-splitting.
static void test_ported_vox_parse_string()
{
    {
        std::string buf = "(p100) my ass is, heavy!(p80 t20) clik.";
        std::array<char *, ::xash::limits::sound_vox_word_max> out{};
        const std::size_t count = parse_string( buf, out );
        const char        *want[] = { "(p100)", "my", "ass", "is", "_comma", "heavy!(p80 t20)", "clik" };
        REQUIRE( count == 7 );
        for( std::size_t i = 0; i < count; ++i )
            CHECK_STREQ( out[i], want[i] );
    }
    {
        std::string buf = "freeman...";
        std::array<char *, ::xash::limits::sound_vox_word_max> out{};
        const std::size_t count = parse_string( buf, out );
        REQUIRE( count == 2 );
        CHECK_STREQ( out[0], "freeman" );
        CHECK_STREQ( out[1], "_period" );
    }
}

// s_vox.c:692-719 Test_VOX_ParseWordParams — 3 cases pin cross-call
// default-carryover as intentional.
static void test_ported_vox_parse_word_params()
{
    VoxWord default_word = make_default_vox_word();
    VoxWord word;
    bool    ret;

    // NOTE: parse_word_params mutates the buffer IN PLACE by inserting a NUL
    // (matching legacy's raw char* mutation) — it does NOT shrink the
    // std::string's own stored size(), so comparisons must go through
    // .c_str() (a strcmp view) rather than std::string::operator==, exactly
    // like the legacy test's TASSERT_STR (a null-terminated-string compare).
    std::string buffer = "heavy!(p80)";
    ret                 = parse_word_params( buffer.data(), word, default_word );
    CHECK_STREQ( buffer.c_str(), "heavy!" );
    CHECK_EQ( word.pitch, 80 );
    CHECK( ret );

    buffer = "(p105)";
    ret    = parse_word_params( buffer.data(), word, default_word );
    CHECK_STREQ( buffer.c_str(), "" );
    CHECK_EQ( word.pitch, 105 );
    CHECK( !ret );

    buffer = "quiet(v50)";
    ret    = parse_word_params( buffer.data(), word, default_word );
    CHECK_STREQ( buffer.c_str(), "quiet" );
    CHECK_EQ( word.pitch, 105 ); // defaulted (carried over from the PREVIOUS call)
    CHECK_EQ( word.volume, 50 );
    CHECK( ret );
}

// ===========================================================================
// 2. Param grammar edge cases.
// ===========================================================================
static void test_parse_word_params_all_fields()
{
    VoxWord dflt = make_default_vox_word();
    VoxWord word;
    std::string buffer = "roomtone(v50 p75 s10 e90 t5)";
    const bool  ret     = parse_word_params( buffer.data(), word, dflt );
    CHECK( ret );
    CHECK_STREQ( buffer.c_str(), "roomtone" ); // see note above re: .c_str() vs operator==
    CHECK_EQ( word.volume, 50 );
    CHECK_EQ( word.pitch, 75 );
    CHECK_EQ( word.start, 10 );
    CHECK_EQ( word.end, 90 );
    CHECK_EQ( word.timecompress, 5 );
}

static void test_parse_word_params_clamping()
{
    VoxWord dflt = make_default_vox_word();
    VoxWord word;
    // v/p clamp to [0,65535]; e clamps to [0,100].
    std::string buffer = "loud(v99999 e150)";
    (void)parse_word_params( buffer.data(), word, dflt );
    CHECK_EQ( word.volume, 65535 );
    CHECK_EQ( word.end, 100 );
}

static void test_parse_word_params_digit_truncation()
{
    // Only the first 7 digit characters are captured (sizeof legacy
    // sznum[8]-1); the 8th+ digit chars are NOT consumed by the numeric
    // scan and fall through to the next command search as ordinary
    // (ignored, non-command) characters.
    VoxWord dflt = make_default_vox_word();
    VoxWord word;
    std::string buffer = "num(v12345678)";
    const bool  ret     = parse_word_params( buffer.data(), word, dflt );
    CHECK( ret );
    CHECK_EQ( word.volume, 65535 ); // atoi("1234567") = 1234567, clamped to UINT16_MAX
}

static void test_parse_word_params_invalid_syntax()
{
    // Ends in ')' but a ')' appears BEFORE any '(' when scanned from the
    // start -> "invalid syntax" (s_vox.c:381-383): returns false, `word` was
    // overwritten with `running_default` but the caller discards it.
    VoxWord dflt = make_default_vox_word();
    VoxWord word;
    std::string buffer = "abc)def)";
    const bool  ret     = parse_word_params( buffer.data(), word, dflt );
    CHECK( !ret );
}

static void test_parse_word_params_no_params()
{
    // Does not end in ')' -> "no special params": word == running_default,
    // buffer untouched, returns true.
    VoxWord dflt = make_default_vox_word();
    dflt.pitch    = 77;
    VoxWord     word;
    std::string buffer = "plainword";
    const bool  ret     = parse_word_params( buffer.data(), word, dflt );
    CHECK( ret );
    CHECK( buffer == "plainword" );
    CHECK_EQ( word.pitch, 77 );
}

static void test_parse_string_group_no_closing_paren()
{
    // H-4 safety deviation pin: an unterminated "(...)" group (no closing
    // ')' before end of string) must not read past the buffer. The word/
    // param split only happens LATER (parse_word_params, on a second pass
    // over this same text) — parse_string itself just runs out of string
    // inside the unterminated group and returns ONE word spanning the
    // WHOLE original text, unmodified (never having found a delimiter to
    // split on), rather than reading past the terminator or crashing.
    std::string buf = "word(unterminated";
    std::array<char *, ::xash::limits::sound_vox_word_max> out{};
    const std::size_t count = parse_string( buf, out );
    REQUIRE( count == 1 );
    CHECK_STREQ( out[0], "word(unterminated" );
}

// ===========================================================================
// 3. TrimStart/TrimEnd vectors (hand-derived), incl. the stereo stride quirk.
// ===========================================================================
static void test_trim_start_mono8()
{
    // TRIM_SAMPLES_BELOW_8 == 2: indices 0,1 are near-silent (0,1), index 2
    // (value 3) is not.
    AudioData wav = make_mono8( { 0, 1, 3, 4, 5 } );
    CHECK_EQ( trim_start( wav, 0 ), 2 );
}

static void test_trim_end_mono8()
{
    // Trailing near-silent run: index4=0, index3=1; index2=3 is not silent.
    AudioData wav = make_mono8( { 5, 4, 3, 1, 0 } );
    CHECK_EQ( trim_end( wav, 5 ), 3 );
}

static void test_trim_start_stereo_stride_quirk()
{
    // STRIDE QUIRK pin (s_vox.c:69-70): each scanned (silent) frame advances
    // the returned index by `channels` (2), not by 1 frame. frame0=(0,0) and
    // frame1=(1,-1) are both silent; frame2=(5,5) is not, but the loop's
    // OWN bound check (`start < samples`) trips first because `start`
    // inflated to 4 after only 2 real frames were examined, against
    // samples=3 — frame2 is never even reached.
    AudioData wav = make_stereo8( { 0, 0, 1, -1, 5, 5 }, /*frames*/ 3 );
    CHECK_EQ( trim_start( wav, 0 ), 4 ); // NOT 2 — the stride inflation is the point of this pin
}

static void test_trim_end_stereo_stride_quirk()
{
    // Mirrors the start-side quirk: trailing frame2=(0,0) and frame1=(1,-1)
    // silent, frame0=(5,5) is not — same 2x-per-frame stride on the way down.
    AudioData wav = make_stereo8( { 5, 5, 1, -1, 0, 0 }, /*frames*/ 3 );
    CHECK_EQ( trim_end( wav, 3 ), -1 ); // end can even go negative (never clamped) — preserved verbatim
}

static void test_trim_start_end_times_normal()
{
    AudioData wav = make_mono8( { 0, 0, 10, 20, 30, 0, 0 } ); // 7 samples
    MixChannel chan{};
    // start percent 0%->frame0 (trimmed to 2), end percent 100%->frame7 (0
    // sentinel would fire only when the CALLER passes end==0; here the
    // caller passes an explicit non-zero end, so no sentinel).
    trim_start_end_times( chan, wav, /*start*/ 0, /*end*/ 7 );
    CHECK_EQ( chan.sample, 2.0 );      // trimmed past the two leading near-silent frames
    CHECK_EQ( chan.forced_end, 5.0 );  // trimmed past the two trailing near-silent frames
}

static void test_trim_start_end_times_zero_sentinel()
{
    // end==0 -> wav.samples - wav.channels (the "to the last frame" sentinel).
    AudioData wav = make_mono8( { 10, 20, 30 } ); // 3 samples, mono (channels=1)
    MixChannel chan{};
    trim_start_end_times( chan, wav, /*start*/ 0, /*end*/ 0 );
    CHECK_EQ( chan.sample, 0.0 );
    CHECK_EQ( chan.forced_end, 2.0 ); // samples(3) - channels(1) = 2, nothing silent to trim further
}

static void test_trim_start_end_times_end_before_start_clamp()
{
    // If the derived end is still < the (possibly-trimmed) start, it is
    // clamped UP to start (s_vox.c:135-136) before being trimmed itself.
    // start=2 is passed directly (index2==50 is loud, so trim_start leaves
    // it unchanged) and end=1 (< start) so the clamp fires; index1==99 is
    // also loud, so the subsequent trim_end(wav,2) leaves it unchanged too
    // — isolating the clamp line itself from any further trimming.
    AudioData wav = make_mono8( { 99, 99, 50, 60, 70 } );
    MixChannel chan{};
    trim_start_end_times( chan, wav, /*start*/ 2, /*end*/ 1 ); // end(1) < start(2)
    CHECK_EQ( chan.sample, 2.0 );
    CHECK_EQ( chan.forced_end, 2.0 );
}

static void test_vox_percent_to_samples()
{
    // Exact case: 50% of 7 -> 3.5 -> round-half-away-from-zero -> 4.
    CHECK_EQ( vox_percent_to_samples( 50, 7 ), 4 );
    // 100% of 100 -> 100.0 exactly.
    CHECK_EQ( vox_percent_to_samples( 100, 100 ), 100 );
    // 0% -> 0.
    CHECK_EQ( vox_percent_to_samples( 0, 12345 ), 0 );
    // Documented float-then-double-round chain, verified against its own
    // formula (the float rounding is the property under test, not an
    // independently-derived value — see mixer.hpp's compute_channel_pitch
    // for the identical justification).
    const int want = static_cast<int>(
        std::round( static_cast<double>( static_cast<float>( 33 ) * 0.01f * static_cast<float>( 100u ) ) ) );
    CHECK_EQ( vox_percent_to_samples( 33, 100 ), want );
}

// ===========================================================================
// 4. FreeWord unconditional-zeroing quirk pin.
// ===========================================================================
static void test_free_word_fields_unconditional_zeroing()
{
    AudioData  dummy = make_mono8( { 1, 2, 3 } );
    MixChannel chan{};
    chan.sample      = 5.0;
    chan.forced_end  = 10.0;
    chan.source      = &dummy;
    chan.flags       = ::xash::abi::k_fl_chan_finished | ::xash::abi::k_fl_chan_use_loop;

    vox_free_word_fields( chan );

    CHECK_EQ( chan.sample, 0.0 );
    CHECK_EQ( chan.forced_end, 0.0 );
    CHECK( chan.source == nullptr );
    CHECK( ( chan.flags & ::xash::abi::k_fl_chan_finished ) == 0 );          // cleared
    CHECK( ( chan.flags & ::xash::abi::k_fl_chan_use_loop ) != 0 );          // NOT touched — only FINISHED is cleared

    // "even on out-of-range/null paths" — calling it again on an already-
    // zeroed channel is a harmless no-op re-zero (the unconditional part
    // never checks any precondition at all).
    vox_free_word_fields( chan );
    CHECK_EQ( chan.sample, 0.0 );
    CHECK_EQ( chan.forced_end, 0.0 );
}

// ===========================================================================
// 5. ModifyPitch float-chain pin, composed with compute_channel_pitch.
// ===========================================================================
static void test_modify_pitch_identity_at_pitch_norm()
{
    CHECK_EQ( modify_pitch( 1.0f, 100 ), 1.0f );
    CHECK_EQ( modify_pitch( 0.75f, 100 ), 0.75f );
}

static void test_modify_pitch_bend()
{
    // word_pitch=150 -> +0.5f exactly representable.
    CHECK_EQ( modify_pitch( 1.0f, 150 ), 1.5f );
    // word_pitch=50 -> -0.5f.
    CHECK_EQ( modify_pitch( 1.0f, 50 ), 0.5f );
}

static void test_compute_channel_pitch_vox_bend_float_chain()
{
    // Non-VOX call sites (word_pitch defaulted) are bit-for-bit unchanged
    // from the S9.3 pin (test_sound_mixer.cpp's
    // test_compute_channel_pitch_float_chain) — re-verified here as the
    // "before" baseline this section composes with.
    CHECK_EQ( compute_channel_pitch( 100.0, 1.0 ), 1.0 );

    // VOX-bent case: basePitch=100 (exact), pitch_mult=1.0, word_pitch=150
    // -> base_f=1.0f, bent_f=modify_pitch(1.0f,150)=1.5f, *1.0f -> 1.5.
    CHECK_EQ( compute_channel_pitch( 100.0, 1.0, 150 ), 1.5 );

    // The float-rounding-BEFORE-the-bend case (the actual parity-critical
    // property): basePitch=95 rounds to float 0.949999988... (not double
    // 0.95) BEFORE VOX_ModifyPitch's bend is added — composing the bend
    // must not "fix" that imprecision away.
    const float  base_f = static_cast<float>( 95.0 * 0.01 );
    CHECK( base_f == 0.95f ); // the float literal 0.95f is bit-identical to this rounding
    const double want = static_cast<double>( modify_pitch( base_f, 110 ) * static_cast<float>( 1.0 ) );
    CHECK_EQ( compute_channel_pitch( 95.0, 1.0, 110 ), want );
    CHECK( compute_channel_pitch( 95.0, 1.0, 110 ) != 95.0 * 0.01 + ( 110 - 100 ) * 0.01 ); // diverges from all-double math
}

// ===========================================================================
// 6. Single-immediate-slot overwrite quirk pin (s_load.c:32-33,312-345).
// ===========================================================================
static void test_immediate_sentence_slot_overwrite_quirk()
{
    ImmediateSentenceSlot slot;
    slot.set( "!first_sentence" );
    CHECK( slot.value() == "!first_sentence" );

    // A second registration BEFORE the first is "consumed" overwrites it —
    // there is no queue, no per-call identity; the first name is simply gone.
    slot.set( "!second_sentence" );
    CHECK( slot.value() == "!second_sentence" );
    CHECK( slot.value() != "!first_sentence" );
}

static void test_immediate_sentence_slot_truncation()
{
    const std::string long_name( 300, 'x' );
    ImmediateSentenceSlot slot;
    slot.set( long_name );
    CHECK_EQ( slot.value().size(), ::xash::limits::sound_vox_immediate_name_max - 1 );
    CHECK( slot.value() == long_name.substr( 0, ::xash::limits::sound_vox_immediate_name_max - 1 ) );
}

// ===========================================================================
// 7. Sentence table parsing (VOX_ReadSentenceFile_) + build_sentence
//    integration, incl. sentence-terminates-on-bad-word.
// ===========================================================================

namespace {

// A minimal IVoxAudioResolver over an in-memory word registry — resolve()
// returns nullptr for any path not present, exercising VOX_LoadWord's
// !word->sfx early-return path exactly like a missing wav file would.
class TestAudioResolver final : public IVoxAudioResolver
{
public:
    std::unordered_map<std::string, AudioData> known;
    std::unordered_map<std::string, bool>      cache_flag; // path -> in_cache to report
    int                                         release_calls = 0;
    std::vector<std::string>                    released_paths_hint; // for debugging only

    const AudioData *resolve( std::string_view path, bool &in_cache ) noexcept override
    {
        auto it = known.find( std::string( path ) );
        if( it == known.end() )
        {
            in_cache = false;
            return nullptr;
        }
        auto cit = cache_flag.find( std::string( path ) );
        in_cache = ( cit != cache_flag.end() ) ? cit->second : false;
        return &it->second;
    }

    void release( const AudioData * ) noexcept override { ++release_calls; }
};

} // namespace

static void test_parse_sentence_file_basic_and_comments()
{
    const std::string text =
        "// a comment line, skipped entirely\n"
        "HELLO Hi there\n"
        "GOODBYE Bye now\n";
    std::vector<VoxSentenceEntry> table;
    parse_sentence_file( as_span( text ), table );
    REQUIRE( table.size() == 2 );
    CHECK( table[0].name == "HELLO" );
    CHECK( table[0].value == "Hi there" );
    CHECK( table[1].name == "GOODBYE" );
    CHECK( table[1].value == "Bye now" );
}

static void test_parse_sentence_file_cap()
{
    // CVOXFILESENTENCEMAX (4096) — one past the cap must be silently dropped.
    std::string text;
    const std::size_t total = ::xash::limits::sound_vox_sentence_table_max + 1;
    for( std::size_t i = 0; i < total; ++i )
        text += "S" + std::to_string( i ) + " v\n";

    std::vector<VoxSentenceEntry> table;
    parse_sentence_file( as_span( text ), table );
    CHECK_EQ( table.size(), ::xash::limits::sound_vox_sentence_table_max );
}

static void test_vox_system_load_and_lookup()
{
    const std::string text = "GREETING hello(p120) world\n";
    VoxSystem          vox;
    vox.load_sentence_file( as_span( text ) );
    CHECK_EQ( vox.sentence_count(), std::size_t{ 1 } );

    auto looked = vox.lookup( "GREETING" );
    REQUIRE( looked.has_value() );
    CHECK( *looked == "hello(p120) world" );
}

static void test_build_sentence_word_list_with_params_and_default_carryover()
{
    const std::string text = "GREETING (p150) hello world\n";
    VoxSystem          vox;
    vox.load_sentence_file( as_span( text ) );

    auto sentence = vox.build_sentence( "GREETING" );
    REQUIRE( sentence.has_value() );
    // "(p150)" is a defaults-only block (no bare word text) — it does NOT
    // become a word entry itself, but updates the running default that
    // BOTH following words inherit (the cross-call default-carryover the
    // legacy embedded test pins, exercised here at the sentence-build
    // level instead of a single parse_word_params call).
    REQUIRE( sentence->words.size() == 2 );
    CHECK_EQ( sentence->words[0].pitch, 150 );
    CHECK_EQ( sentence->words[1].pitch, 150 );
    // S9.6 lazy-resolution fix: build_sentence() never resolves audio — only
    // the "<dir>/<word>" path is recorded; audio stays null until the word
    // is actually reached during playback (load_word/next_word).
    CHECK( sentence->words[0].path == "vox/hello" );
    CHECK( sentence->words[1].path == "vox/world" );
    CHECK( sentence->words[0].audio == nullptr );
    CHECK( sentence->words[1].audio == nullptr );

    TestAudioResolver resolver;
    resolver.known["vox/hello"] = make_mono8( { 10, 20 } );
    resolver.known["vox/world"] = make_mono8( { 30, 40 } );

    MixChannel chan{};
    chan.leftvol = chan.rightvol = 1;
    vox.bind_channel( chan, std::move( *sentence ), &resolver );
    // bind_channel() loads word 0 (VOX_LoadWord) -> lazy resolve happens HERE.
    CHECK( chan.source == &resolver.known["vox/hello"] );

    // Advance to word 1 -> lazy resolve happens at THIS call, not earlier.
    chan.flags |= ::xash::abi::k_fl_chan_finished;
    CHECK( vox.next_word( chan ) );
    CHECK( chan.source == &resolver.known["vox/world"] );

    vox.unbind_channel( chan );
}

static void test_build_sentence_unknown_name_fails()
{
    VoxSystem vox;
    CHECK( !vox.build_sentence( "NO_SUCH_SENTENCE" ).has_value() );
}

static void test_sentence_terminates_on_bad_word()
{
    // Three words; the MIDDLE one fails to resolve (e.g. a missing .wav on
    // disk in the real S9.6 wiring). build_sentence still builds all 3
    // entries (legacy never checks S_FindName's result before continuing
    // the loop, s_vox.c:507) — but playback must terminate exactly at that
    // word and never reach the third, even though the third IS resolvable.
    const std::string text = "SENT one two three\n";
    VoxSystem          vox;
    vox.load_sentence_file( as_span( text ) );

    auto sentence = vox.build_sentence( "SENT" );
    REQUIRE( sentence.has_value() );
    REQUIRE( sentence->words.size() == 3 );
    CHECK( sentence->words[0].path == "vox/one" );
    CHECK( sentence->words[1].path == "vox/two" );
    CHECK( sentence->words[2].path == "vox/three" );
    // Nothing resolved yet (S9.6 lazy-resolution fix).
    CHECK( sentence->words[0].audio == nullptr );
    CHECK( sentence->words[1].audio == nullptr );
    CHECK( sentence->words[2].audio == nullptr );

    TestAudioResolver resolver;
    resolver.known["vox/one"]   = make_mono8( { 1, 2 } );
    // "vox/two" intentionally absent -> resolve() returns nullptr.
    resolver.known["vox/three"] = make_mono8( { 5, 6 } );

    MixChannel chan{};
    chan.leftvol = chan.rightvol = 1;
    vox.bind_channel( chan, std::move( *sentence ), &resolver );

    // Word 0 loaded successfully: not sentence-finished, source bound.
    CHECK( ( chan.flags & ::xash::abi::k_fl_chan_sentence_finished ) == 0 );
    CHECK( chan.source == &resolver.known["vox/one"] );

    // Simulate word 0 finishing (as vox_mix_channel_to_buffer would once
    // FL_CHAN_FINISHED is observed) and advance — lands on the bad word.
    // Exactly like the real vox_mix_channel_to_buffer loop, the Mixer NEVER
    // calls next_word() again once FL_CHAN_SENTENCE_FINISHED is set (its own
    // while-condition gates on it) — so word 2 ("three", resolvable) is
    // provably unreached: only ONE release() call happens (for word 0);
    // word 1 was never resolved (nothing to release) and word 2 was never
    // even looked at.
    chan.flags |= ::xash::abi::k_fl_chan_finished;
    const bool advanced = vox.next_word( chan );
    CHECK( !advanced );
    CHECK( ( chan.flags & ::xash::abi::k_fl_chan_sentence_finished ) != 0 );
    CHECK( chan.source == nullptr ); // vox_free_word_fields nulled it; load_word never rebinds on failure
    CHECK_EQ( resolver.release_calls, 1 );

    vox.unbind_channel( chan );
}

// FreeWord's "even on out-of-range/null paths" unconditional-zeroing quirk,
// exercised through the FULL driver (VoxSystem::next_word), not just the
// standalone vox_free_word_fields helper: a channel whose word_index has
// ALREADY run past the end of its (single-word) sentence still gets its
// fields unconditionally re-zeroed on a further next_word() call, rather
// than skipping the zero step because there is "nothing there".
static void test_free_word_quirk_through_driver_out_of_range_index()
{
    AudioData   only_word = make_mono8( { 50, 60 } );
    VoxSentence sentence;
    sentence.words.push_back( VoxWord{ &only_word, 100, 100, 0, 0, 100, false } );

    VoxSystem  vox;
    MixChannel chan{};
    vox.bind_channel( chan, std::move( sentence ), /*resolver*/ nullptr );
    CHECK( ( chan.flags & ::xash::abi::k_fl_chan_sentence_finished ) == 0 );

    // Word 0 "finishes" -> next_word() moves word_index to 1, which is
    // already >= words.size()(1): load_word's bounds check fires, leaving
    // SENTENCE_FINISHED set (the natural end-of-sentence case).
    chan.flags |= ::xash::abi::k_fl_chan_finished;
    CHECK( !vox.next_word( chan ) );
    CHECK( ( chan.flags & ::xash::abi::k_fl_chan_sentence_finished ) != 0 );
    CHECK( chan.source == nullptr );

    // A further next_word() call is something the real Mixer would never
    // issue (its loop gates on SENTENCE_FINISHED) — issued here directly to
    // pin the quirk: free_word() still runs vox_free_word_fields()
    // UNCONDITIONALLY first, even though word_index(1) is already out of
    // range, before its own bounds check turns the rest into a no-op.
    chan.sample     = 123.0; // poison the fields to prove they get re-zeroed
    chan.forced_end = 456.0;
    CHECK( !vox.next_word( chan ) );
    CHECK_EQ( chan.sample, 0.0 );
    CHECK_EQ( chan.forced_end, 0.0 );
    CHECK( chan.source == nullptr );

    vox.unbind_channel( chan );
}

// ===========================================================================
// 8. Full 3-word sentence mix through vox_mix_channel_to_buffer (byte-pinned).
// ===========================================================================
static void test_full_sentence_mix_three_words()
{
    AudioData word0 = make_mono8( { 10, 20 } );
    AudioData word1 = make_mono8( { 30, 40 } );
    AudioData word2 = make_mono8( { 50, 60 } );

    VoxSentence sentence;
    sentence.words.push_back( VoxWord{ &word0, 100, 100, 0, 0, 100, false } );
    sentence.words.push_back( VoxWord{ &word1, 100, 100, 0, 0, 100, false } );
    sentence.words.push_back( VoxWord{ &word2, 100, 100, 0, 0, 100, false } );

    VoxSystem  vox;
    MixChannel chan{};
    chan.leftvol   = 1;
    chan.rightvol  = 1;
    chan.is_sentence = true;

    vox.bind_channel( chan, std::move( sentence ), /*resolver*/ nullptr );
    REQUIRE( ( chan.flags & ::xash::abi::k_fl_chan_sentence_finished ) == 0 );

    portable_samplepair_t pbuf[6] = {};
    const int written = vox_mix_channel_to_buffer( pbuf, chan, /*num_samples*/ 6, /*out_rate*/ 44100,
                                                   /*pitch*/ 1.0, /*lerping*/ false, vox );

    CHECK_EQ( written, 6 );
    // Hand-derived: rate==1 (src.rate==out_rate), leftvol==rightvol==1 (shift
    // 0, 8-bit) -> each pair equals the raw sample value on both channels.
    const int want[6] = { 10, 20, 30, 40, 50, 60 };
    for( int i = 0; i < 6; ++i )
    {
        if( pbuf[i].left == want[i] && pbuf[i].right == want[i] )
            ++g_pass;
        else
        {
            ++g_fail;
            std::printf( "FAIL [full sentence mix idx %d]: got {%d,%d} want %d\n", i, pbuf[i].left, pbuf[i].right,
                        want[i] );
        }
    }

    CHECK( ( chan.flags & ::xash::abi::k_fl_chan_sentence_finished ) != 0 );

    vox.unbind_channel( chan );
}

int main()
{
    // FIX B (chunks-8/9/10 close-out): VoxSystem::load_sentence_file() now
    // carries a REAL assert_thread_role(Main) (it is a T_Main registration-
    // time entry, not under the channel-array-owner conditional the rest of
    // VoxSystem uses) — register the role here, matching every sibling sound
    // test file's established convention (test_sound_device.cpp,
    // test_sound_lifecycle.cpp, test_sound_entry.cpp).
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    // 1. Ported legacy XASH_ENGINE_TESTS cases.
    RUN_TEST( test_ported_vox_get_directory );
    RUN_TEST( test_ported_vox_lookup_string );
    RUN_TEST( test_ported_vox_parse_string );
    RUN_TEST( test_ported_vox_parse_word_params );

    // 2. Param grammar edge cases.
    RUN_TEST( test_parse_word_params_all_fields );
    RUN_TEST( test_parse_word_params_clamping );
    RUN_TEST( test_parse_word_params_digit_truncation );
    RUN_TEST( test_parse_word_params_invalid_syntax );
    RUN_TEST( test_parse_word_params_no_params );
    RUN_TEST( test_parse_string_group_no_closing_paren );

    // 3. TrimStart/TrimEnd.
    RUN_TEST( test_trim_start_mono8 );
    RUN_TEST( test_trim_end_mono8 );
    RUN_TEST( test_trim_start_stereo_stride_quirk );
    RUN_TEST( test_trim_end_stereo_stride_quirk );
    RUN_TEST( test_trim_start_end_times_normal );
    RUN_TEST( test_trim_start_end_times_zero_sentinel );
    RUN_TEST( test_trim_start_end_times_end_before_start_clamp );
    RUN_TEST( test_vox_percent_to_samples );

    // 4. FreeWord zeroing pin.
    RUN_TEST( test_free_word_fields_unconditional_zeroing );

    // 5. ModifyPitch float-chain.
    RUN_TEST( test_modify_pitch_identity_at_pitch_norm );
    RUN_TEST( test_modify_pitch_bend );
    RUN_TEST( test_compute_channel_pitch_vox_bend_float_chain );

    // 6. Single-immediate-slot quirk.
    RUN_TEST( test_immediate_sentence_slot_overwrite_quirk );
    RUN_TEST( test_immediate_sentence_slot_truncation );

    // 7. Sentence table + build_sentence + bad-word termination.
    RUN_TEST( test_parse_sentence_file_basic_and_comments );
    RUN_TEST( test_parse_sentence_file_cap );
    RUN_TEST( test_vox_system_load_and_lookup );
    RUN_TEST( test_build_sentence_word_list_with_params_and_default_carryover );
    RUN_TEST( test_build_sentence_unknown_name_fails );
    RUN_TEST( test_sentence_terminates_on_bad_word );
    RUN_TEST( test_free_word_quirk_through_driver_out_of_range_index );

    // 8. Full sentence mix (byte-pinned).
    RUN_TEST( test_full_sentence_mix_three_words );

    std::printf( "sound_vox: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
