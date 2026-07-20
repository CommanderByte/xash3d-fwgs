// xash3dpp — VOX sentence word-sequencer (Chunk 9, slice S9.4). PARITY-CRITICAL.
// Legacy reference: engine/client/sound/s_vox.c (see vox.hpp for the per-
// function line map) + engine/client/sound/s_load.c:32-33,312-345
// (ImmediateSentenceSlot). Every parse/arithmetic step is a verbatim port —
// see vox.hpp's per-function doc comments for the specific quirks preserved
// (the amplitude-trim stereo stride, FreeWord's unconditional zeroing, the
// cross-call VOX_ParseWordParams default-carryover, the single-immediate-
// slot overwrite). Legacy C engine is REFERENCE-ONLY.
//
// @thread-safety: see vox.hpp's file header — mirrors the Mixer's
// compliance-allow(thread-assert) posture (T_AudioDecoder, unasserted this
// slice; networking-boundary precedent, context.cpp:81).

#include <xash3dpp/private/sound/vox.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <xash3dpp/utilities/string.hpp>

namespace xash::sound {

namespace {

// bound(min, num, max) verbatim (public/xash3d_mathlib.h:141) — duplicated
// locally per the mixer.cpp precedent (no shared house `bound()` utility).
[[nodiscard]] constexpr int bound_int( int lo, int v, int hi ) noexcept
{
    return v >= lo ? ( v < hi ? v : hi ) : lo;
}

// The two synthesized pseudo-words a mid-sentence '.'/',' inserts
// (VOX_ParseString, s_vox.c:29,333-334). Legacy casts away const on two
// `static const char *` literals to store them into the (mutable) word-
// pointer array; since VOX_ParseWordParams only ever WRITES through a word
// pointer when that word's text ends in ')' (s_vox.c:376), and neither
// pseudo-word ever does, that cast is never actually exercised — but casting
// away const on a true string-literal's pointer is still UB by the letter of
// the C++ standard (literals may be placed in read-only memory). This port
// avoids the cast entirely by giving each pseudo-word its own ordinary
// mutable char array instead of a string literal: same "never actually
// written" guarantee, zero UB exposure, no shared/global mutable state
// beyond what the legacy file-scope statics already had.
char g_vox_period_word[] = "_period";
char g_vox_comma_word[]  = "_comma";

// Q_isdigit's WHOLE-STRING semantics (public/crtlib.h:185-188): non-empty
// AND every character satisfies isdigit — NOT just the first character.
[[nodiscard]] bool is_all_digits( std::string_view s ) noexcept
{
    if( s.empty() )
        return false;
    for( char c : s )
        if( !std::isdigit( static_cast<unsigned char>( c ) ) )
            return false;
    return true;
}

// S_ShouldTrimSample8/16 (s_vox.c:31-51) — symmetric-AND-across-channels
// silence test at the given threshold.
[[nodiscard]] bool should_trim_sample8( const std::int8_t *buf, int channels ) noexcept
{
    if( std::abs( static_cast<int>( buf[0] ) ) > k_vox_trim_below_8 )
        return false;
    if( channels >= 2 && std::abs( static_cast<int>( buf[1] ) ) > k_vox_trim_below_8 )
        return false;
    return true;
}

[[nodiscard]] bool should_trim_sample16( const std::int16_t *buf, int channels ) noexcept
{
    if( std::abs( static_cast<int>( buf[0] ) ) > k_vox_trim_below_16 )
        return false;
    if( channels >= 2 && std::abs( static_cast<int>( buf[1] ) ) > k_vox_trim_below_16 )
        return false;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// parse_sentence_file (VOX_ReadSentenceFile_, s_vox.c:523-568).
// ---------------------------------------------------------------------------
void parse_sentence_file( std::span<const std::byte> data, std::vector<VoxSentenceEntry> &out ) noexcept
{
    const char *base = reinterpret_cast<const char *>( data.data() );
    const char *p    = base;
    const char *last = base + data.size();

    while( p < last )
    {
        if( out.size() >= ::xash::limits::sound_vox_sentence_table_max )
            break; // legacy: bare `break`, no error (the CVOXFILESENTENCEMAX cap)

        for( ; p < last && ( *p == '\n' || *p == '\r' || *p == '\t' || *p == ' ' ); ++p )
        {
        }

        // H-4 deviation (documented, not silent): legacy dereferences `*p`
        // here UNCONDITIONALLY, relying on FS_LoadFile's guaranteed trailing
        // NUL beyond `size` (so `p == last` still reads a safe 0 byte). A
        // caller-supplied span has no such guarantee — stop cleanly at the
        // boundary instead of reading past it. The only behaviour this could
        // possibly diverge from is legacy's degenerate empty name/value
        // entry at exact end-of-file-after-trailing-whitespace, which is not
        // a meaningful sentence.
        if( p >= last )
            break;

        const char *name  = nullptr;
        const char *value = nullptr;
        std::size_t name_len = 0, value_len = 0;

        if( *p != '/' )
        {
            name = p;
            for( ; p < last && *p != ' ' && *p != '\t'; ++p )
            {
            }
            name_len = static_cast<std::size_t>( p - name );

            if( p < last )
                ++p; // *p++ = 0 (skip the name/value delimiter)

            value = p;
        }

        for( ; p < last && *p != '\n' && *p != '\r'; ++p )
        {
        }
        if( value != nullptr )
            value_len = static_cast<std::size_t>( p - value );

        if( p < last )
            ++p; // *p++ = 0 (skip the newline)

        if( name != nullptr )
            out.push_back( VoxSentenceEntry{ std::string( name, name_len ), std::string( value, value_len ) } );
    }
}

// ---------------------------------------------------------------------------
// vox_lookup_string (VOX_LookupString, s_vox.c:257-298).
// ---------------------------------------------------------------------------
std::optional<std::string_view> vox_lookup_string( std::span<const VoxSentenceEntry> table,
                                                    std::string_view pszin ) noexcept
{
    // "check if we are an immediate sentence" (s_vox.c:263-267).
    if( !pszin.empty() && pszin.front() == '#' )
        return pszin.substr( 1 );

    int idx = -1;

    // "check if we received an index" (s_vox.c:270-276).
    if( is_all_digits( pszin ) )
    {
        const int parsed = ::xash::utilities::atoi( pszin );
        idx               = ( parsed >= 0 && static_cast<std::size_t>( parsed ) < table.size() ) ? parsed : -1;
    }

    // "last hope: find it in sentences array" (s_vox.c:279-286) — ONLY when
    // the numeric branch did not already resolve an index.
    if( idx == -1 )
    {
        for( std::size_t i = 0; i < table.size(); ++i )
        {
            if( ::xash::utilities::ci_equal( pszin, table[i].name ) )
            {
                idx = static_cast<int>( i );
                break;
            }
        }
    }

    if( idx < 0 )
        return std::nullopt; // "not found, exit" (s_vox.c:289-290)

    // Skip leading spaces/tabs (s_vox.c:294-295) — the value was stored raw.
    std::string_view value = table[static_cast<std::size_t>( idx )].value;
    const std::size_t start = value.find_first_not_of( " \t" );
    if( start == std::string_view::npos )
        return std::string_view{};
    return value.substr( start );
}

// ---------------------------------------------------------------------------
// vox_get_directory (VOX_GetDirectory, s_vox.c:223-255).
// ---------------------------------------------------------------------------
std::optional<std::string_view> vox_get_directory( std::string_view psz, std::string &out_dir,
                                                    std::size_t dir_max ) noexcept
{
    // HACKHACK: leading '/' silently skipped (s_vox.c:228-232).
    if( !psz.empty() && psz.front() == '/' )
        psz.remove_prefix( 1 );

    const std::size_t pos = psz.rfind( '/' );
    if( pos == std::string_view::npos )
    {
        out_dir = "vox/";
        return psz;
    }

    const std::size_t len = pos + 1; // includes the trailing '/' (s_vox.c:243)
    if( len > dir_max )
        return std::nullopt; // "invalid directory in: %s" (s_vox.c:245-249)

    out_dir = std::string( psz.substr( 0, len ) );
    return psz.substr( len );
}

// ---------------------------------------------------------------------------
// parse_string (VOX_ParseString, s_vox.c:300-352).
// ---------------------------------------------------------------------------
std::size_t parse_string( std::string &buf, std::array<char *, ::xash::limits::sound_vox_word_max> &out ) noexcept
{
    std::size_t i   = 0;
    char       *psz = buf.data(); // guaranteed NUL-terminated at buf[buf.size()]

    out[i++] = psz;

    while( i < ::xash::limits::sound_vox_word_max )
    {
        // skip to next word
        for( ; *psz && *psz != ' ' && *psz != '.' && *psz != ',' && *psz != '('; ++psz )
        {
        }

        // skip anything in between ( and )
        if( *psz == '(' )
        {
            for( ; *psz && *psz != ')'; ++psz )
            {
            }
            // H-4 deviation (documented, not silent): legacy unconditionally
            // does `psz++` here even when the scan above stopped at the NUL
            // terminator (no closing ')' before end-of-string) — reading the
            // byte AFTER that terminator on the very next line. That is
            // in-bounds for legacy's stack buffer in practice but is
            // genuinely undefined behaviour for a std::string's storage.
            // Only advance past a REAL ')'; an unterminated group instead
            // falls straight into the `if( !*psz ) return i;` below.
            if( *psz == ')' )
                ++psz;
        }

        if( !*psz )
            return i;

        // . and , are special but if not end of string
        if( ( *psz == '.' || *psz == ',' ) && psz[1] != '\n' && psz[1] != '\r' && psz[1] != '\0' )
        {
            out[i++] = ( *psz == '.' ) ? g_vox_period_word : g_vox_comma_word;

            if( i >= ::xash::limits::sound_vox_word_max )
                return i;
        }

        *psz++ = 0;

        for( ; *psz && ( *psz == '.' || *psz == ' ' || *psz == ',' ); ++psz )
        {
        }

        if( !*psz )
            return i;

        out[i++] = psz;
    }

    return i;
}

// ---------------------------------------------------------------------------
// parse_word_params (VOX_ParseWordParams, s_vox.c:363-442).
// ---------------------------------------------------------------------------
bool parse_word_params( char *psz, VoxWord &word, VoxWord &running_default ) noexcept
{
    char *pszsave = psz;

    word = running_default; // *pvoxword = *default_voxword;

    const std::size_t len = std::strlen( psz );
    if( len == 0 )
        return false;

    // no special params
    if( psz[len - 1] != ')' )
        return true;

    for( ; *psz != '(' && *psz != ')'; ++psz )
    {
    }

    // invalid syntax
    if( *psz == ')' )
        return false;

    // split filename and params
    *psz++ = '\0';

    for( ;; )
    {
        for( ; *psz && *psz != 'v' && *psz != 'p' && *psz != 's' && *psz != 'e' && *psz != 't'; ++psz )
        {
            if( *psz == ')' )
                break;
        }

        const char command = *psz++;

        if( !std::isdigit( static_cast<unsigned char>( *psz ) ) )
            break;

        const char *digit_start = psz;
        std::size_t k           = 0;
        for( ; k < 7 && std::isdigit( static_cast<unsigned char>( *psz ) ); ++k, ++psz )
        {
        }

        const int value = ::xash::utilities::atoi( std::string_view( digit_start, k ) );
        switch( command )
        {
        case 'e': word.end          = static_cast<std::uint8_t>( bound_int( 0, value, 100 ) ); break;
        case 'p': word.pitch        = static_cast<std::uint16_t>( bound_int( 0, value, 65535 ) ); break;
        case 's': word.start        = static_cast<std::uint8_t>( bound_int( 0, value, 100 ) ); break;
        case 't': word.timecompress = static_cast<std::uint8_t>( bound_int( 0, value, 100 ) ); break;
        case 'v': word.volume       = static_cast<std::uint16_t>( bound_int( 0, value, 65535 ) ); break;
        default: break;
        }
    }

    // no actual word but new defaults
    if( std::strlen( pszsave ) == 0 )
    {
        running_default = word;
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Amplitude trim (S_TrimStart/S_TrimEnd/S_TrimStartEndTimes, s_vox.c:31-139).
// ---------------------------------------------------------------------------
int trim_start( const AudioData &wav, int start ) noexcept
{
    if( wav.type != AudioFormatType::Pcm )
        return start;

    const int channels = wav.channels;
    const int width     = wav.width;
    const int samples   = static_cast<int>( wav.samples );

    if( width == 1 )
    {
        const std::int8_t *data =
            reinterpret_cast<const std::int8_t *>( wav.buffer.data() ) + static_cast<std::ptrdiff_t>( channels ) * start;
        for( int i = 0; i < k_vox_trim_scan_max && start < samples; ++i )
        {
            if( !should_trim_sample8( data, channels ) )
                break;
            start += channels;
            data += channels;
        }
    }
    else if( width == 2 )
    {
        const std::int16_t *data = reinterpret_cast<const std::int16_t *>( wav.buffer.data() )
                                    + static_cast<std::ptrdiff_t>( channels ) * start;
        for( int i = 0; i < k_vox_trim_scan_max && start < samples; ++i )
        {
            if( !should_trim_sample16( data, channels ) )
                break;
            start += channels;
            data += channels;
        }
    }

    return start;
}

int trim_end( const AudioData &wav, int end ) noexcept
{
    if( wav.type != AudioFormatType::Pcm )
        return end;

    const int channels = wav.channels;
    const int width     = wav.width;

    // Guard the initial pointer computation on end>0 (H-4 discipline): legacy
    // computes `&wav->buffer[channels*width*(end-1)]` unconditionally, which
    // is a negative/out-of-bounds offset when end<=0 — never DEREFERENCED in
    // that case (the loop condition `end>0` fails immediately), but computing
    // it is still pointer-arithmetic UB this port avoids without changing the
    // return value (end, unchanged, exactly as legacy would also return).
    if( width == 1 && end > 0 )
    {
        const std::int8_t *data = reinterpret_cast<const std::int8_t *>( wav.buffer.data() )
                                   + static_cast<std::ptrdiff_t>( channels ) * ( end - 1 );
        for( int i = 0; i < k_vox_trim_scan_max && end > 0; ++i )
        {
            if( !should_trim_sample8( data, channels ) )
                break;
            end -= channels;
            data -= channels;
        }
    }
    else if( width == 2 && end > 0 )
    {
        const std::int16_t *data = reinterpret_cast<const std::int16_t *>( wav.buffer.data() )
                                    + static_cast<std::ptrdiff_t>( channels ) * ( end - 1 );
        for( int i = 0; i < k_vox_trim_scan_max && end > 0; ++i )
        {
            if( !should_trim_sample16( data, channels ) )
                break;
            end -= channels;
            data -= channels;
        }
    }

    return end;
}

void trim_start_end_times( MixChannel &chan, const AudioData &wav, int start, int end ) noexcept
{
    start        = trim_start( wav, start );
    chan.sample  = static_cast<double>( start );

    // don't overrun the buffer while trimming end
    if( end == 0 )
        end = static_cast<int>( wav.samples ) - static_cast<int>( wav.channels );

    if( end < start )
        end = start;

    chan.forced_end = static_cast<double>( trim_end( wav, end ) );
}

int vox_percent_to_samples( int percent, std::uint32_t samples ) noexcept
{
    // round( percent * 0.01f * data->samples ) (s_vox.c:167) — float multiply
    // (0.01f is a float literal; `samples`, uint32, converts to float to
    // match it — NOT double), THEN widen to double for round()'s prototype.
    const float value_f = static_cast<float>( percent ) * 0.01f * static_cast<float>( samples );
    return static_cast<int>( std::round( static_cast<double>( value_f ) ) );
}

// ---------------------------------------------------------------------------
// vox_free_word_fields (the unconditional half of VOX_FreeWord, s_vox.c:170-188).
// ---------------------------------------------------------------------------
void vox_free_word_fields( MixChannel &chan ) noexcept
{
    // TODO (verbatim, s_vox.c:172): "don't set random fields to zero lol,
    // was memset before" — preserved, not fixed.
    chan.sample      = 0.0;
    chan.forced_end  = 0.0;
    chan.flags      &= ~::xash::abi::k_fl_chan_finished;
    chan.source      = nullptr; // ch->data = NULL
}

// ---------------------------------------------------------------------------
// ImmediateSentenceSlot
// ---------------------------------------------------------------------------
void ImmediateSentenceSlot::set( std::string_view name ) noexcept
{
    // Q_strncpy(dst, src, sizeof(dst)) — always truncates, always NUL-
    // terminates (s_load.c:321); MAX_STRING == sound_vox_immediate_name_max.
    const std::size_t cap = ::xash::limits::sound_vox_immediate_name_max > 0
                                 ? ::xash::limits::sound_vox_immediate_name_max - 1
                                 : 0;
    name_.assign( name.substr( 0, std::min( name.size(), cap ) ) );
}

// ---------------------------------------------------------------------------
// VoxSystem
// ---------------------------------------------------------------------------

void VoxSystem::load_sentence_file( std::span<const std::byte> data )
{
    // VOX_ReadSentenceFile calls VOX_Shutdown() before parsing (s_vox.c:575).
    clear();
    parse_sentence_file( data, sentences_ );
}

std::optional<VoxSentence> VoxSystem::build_sentence( std::string_view name, IVoxAudioResolver &resolver ) const
{
    if( name.empty() )
        return std::nullopt; // !pszin guard (s_vox.c:454-455)

    const std::optional<std::string_view> looked_up = lookup( name );
    if( !looked_up )
        return std::nullopt; // "no sentence named %s" (s_vox.c:466-471)

    std::string                           dir;
    const std::optional<std::string_view> word_text = vox_get_directory( *looked_up, dir );
    if( !word_text )
        return std::nullopt; // "failed getting directory" (s_vox.c:475-479)

    if( word_text->size() >= ::xash::limits::sound_vox_sentence_text_max )
        return std::nullopt; // "sentence is too long" (s_vox.c:481-485)

    std::string buffer( *word_text ); // Q_strncpy into the mutable scratch buffer

    std::array<char *, ::xash::limits::sound_vox_word_max> raw_words{};
    const std::size_t num_words = parse_string( buffer, raw_words );

    VoxWord running_default = make_default_vox_word();

    VoxSentence sentence;
    sentence.words.reserve( num_words );

    for( std::size_t i = 0; i < num_words; ++i )
    {
        VoxWord word{};
        if( !parse_word_params( raw_words[i], word, running_default ) )
            continue; // defaults-only block OR a syntax error (s_vox.c:493-498)

        const std::string path = dir + raw_words[i]; // szpath + word-name (s_vox.c:500)

        bool in_cache = false;
        word.audio    = resolver.resolve( path, in_cache );
        word.in_cache = in_cache;

        sentence.words.push_back( word );
    }

    return sentence;
}

bool VoxSystem::load_word( MixChannel &chan, ChannelState &state ) noexcept
{
    // SetBits( ch->flags, FL_CHAN_SENTENCE_FINISHED ); — unconditional, first.
    chan.flags |= ::xash::abi::k_fl_chan_sentence_finished;

    // word_index<0||>=CVOXWORDMAX (s_vox.c:145) collapses to the vector bound
    // here: legacy's per-sentence array is sized (num_words+1) with a null-
    // sfx terminator at index num_words, so word_index can validly reach
    // num_words (the terminator, still < CVOXWORDMAX) but never beyond it in
    // practice (the outer word-advance loop stops once SENTENCE_FINISHED is
    // set, which happens exactly when the terminator is loaded) — this
    // port's std::vector::size() == num_words replaces the terminator
    // entirely, so `word_index >= words.size()` is the exact same "no more
    // words" boundary.
    if( state.word_index >= state.sentence.words.size() )
        return false;

    VoxWord &word = state.sentence.words[state.word_index];

    if( word.audio == nullptr )
        return false; // !word->sfx (s_vox.c:150-151) — sentence terminates at this word

    // (S9.6 collapses S_LoadSound's lazy decode-on-mix here too — word.audio
    // is already the resolved AudioData in this self-contained slice; see
    // IVoxAudioResolver's doc comment.)

    chan.flags &= ~::xash::abi::k_fl_chan_sentence_finished; // ClearBits
    chan.source = word.audio;                                 // ch->data = data

    int start = word.start;
    int end   = word.end;
    if( end <= start )
        end = 0;

    trim_start_end_times( chan, *word.audio, vox_percent_to_samples( start, word.audio->samples ),
                          vox_percent_to_samples( end, word.audio->samples ) );

    // Cache the current word's timecompress/pitch onto the channel — the
    // same pattern S9.3 already established for timecompress; vox_pitch is
    // this slice's addition (see mixer.hpp).
    chan.timecompress = word.timecompress;
    chan.vox_pitch     = word.pitch;

    return true;
}

void VoxSystem::free_word( MixChannel &chan, ChannelState &state ) noexcept
{
    vox_free_word_fields( chan ); // the unconditional zeroing (s_vox.c:172-175)

    if( state.word_index >= state.sentence.words.size() ) // word_index<0||>=CVOXWORDMAX (see load_word note)
        return;

    VoxWord &word = state.sentence.words[state.word_index];

    if( word.audio == nullptr || word.in_cache ) // !word->sfx || FL_VOXWORD_IN_CACHE
        return;

    if( state.resolver != nullptr )
        state.resolver->release( word.audio ); // FS_FreeSound(word->sfx->cache)

    word.audio = nullptr; // word->sfx->cache = NULL; word->sfx = NULL (collapsed to one pointer)
}

void VoxSystem::bind_channel( MixChannel &chan, VoxSentence sentence, IVoxAudioResolver *resolver )
{
    // "free any existing words from a previous sentence on this channel"
    // (s_vox.c:457-462).
    unbind_channel( chan );

    ChannelState state;
    state.sentence   = std::move( sentence );
    state.word_index = 0;
    state.resolver   = resolver;

    auto [it, inserted] = bound_.emplace( &chan, std::move( state ) );
    (void)inserted;

    chan.is_sentence = true; // ch->words = ... (non-null marker; s_mix.c:395's `if(ch->words)` test)

    (void)load_word( chan, it->second ); // VOX_LoadWord(ch) for word_index=0 (s_vox.c:519-520)
}

void VoxSystem::unbind_channel( MixChannel &chan ) noexcept
{
    auto it = bound_.find( &chan );
    if( it == bound_.end() )
        return;

    free_word( chan, it->second ); // VOX_FreeWord(ch) (s_vox.c:459)
    bound_.erase( it );            // Mem_Free2(&ch->words) (s_vox.c:460)

    chan.is_sentence = false;
    chan.vox_pitch    = k_pitch_norm;
}

bool VoxSystem::next_word( MixChannel &chan ) noexcept
{
    auto it = bound_.find( &chan );
    if( it == bound_.end() )
    {
        // No bound sentence — should not happen (the Mixer only routes
        // is_sentence channels with a non-null vox_ hook here). Fail safe:
        // end the sentence rather than mixing against stale state.
        chan.flags |= ::xash::abi::k_fl_chan_sentence_finished;
        return false;
    }

    ChannelState &state = it->second;

    // VOX_FreeWord(chan); chan->word_index++; VOX_LoadWord(chan); (s_mix.c:
    // 296-298). The legacy `chan->sfx = chan->words[chan->word_index].sfx;`
    // bookkeeping line (s_mix.c:301) has no MixChannel-side field to mirror
    // (MixChannel tracks the resolved AudioData via `source`, not an sfx_t
    // indirection) — S9.6's channel_t bridging owns that assignment if/when
    // it needs one.
    free_word( chan, state );
    ++state.word_index;
    return load_word( chan, state );
}

} // namespace xash::sound
