// xash3dpp — string utilities implementation
// Legacy reference: public/crtlib.c

#include <xash3dpp/utilities/string.hpp>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace xash::utilities {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Portable ASCII tolower — avoids locale overhead and UB on signed char.
static constexpr int ascii_lower( int c ) noexcept
{
    return ( c >= 'A' && c <= 'Z' ) ? c + ( 'a' - 'A' ) : c;
}

static std::string_view skip_spaces( std::string_view s ) noexcept
{
    const auto pos = s.find_first_not_of( ' ' );
    return ( pos != std::string_view::npos ) ? s.substr( pos ) : std::string_view{};
}

// ---------------------------------------------------------------------------
// strncpy
// ---------------------------------------------------------------------------

char *strncpy( char *dst, const char *src, std::size_t size ) noexcept
{
    if( !dst || !size ) return dst;

    std::size_t i = 0;
    if( src )
        for( ; i < size - 1 && src[i]; ++i )
            dst[i] = src[i];
    dst[i] = '\0';
    return dst;
}

// ---------------------------------------------------------------------------
// strcmp / stricmp / strnicmp
// ---------------------------------------------------------------------------

int strcmp( const char *a, const char *b ) noexcept
{
    if( a == b ) return 0;
    if( !a )     return -1;
    if( !b )     return  1;

    for( ;; )
    {
        const int ca = static_cast<unsigned char>( *a++ );
        const int cb = static_cast<unsigned char>( *b++ );
        if( ca != cb ) return ca - cb;
        if( ca == 0  ) return 0;
    }
}

int stricmp( const char *a, const char *b ) noexcept
{
    if( a == b ) return 0;
    if( !a )     return -1;
    if( !b )     return  1;

    for( ;; )
    {
        const int ca = ascii_lower( static_cast<unsigned char>( *a++ ) );
        const int cb = ascii_lower( static_cast<unsigned char>( *b++ ) );
        if( ca != cb ) return ca - cb;
        if( ca == 0  ) return 0;
    }
}

int strnicmp( const char *a, const char *b, std::size_t n ) noexcept
{
    if( !n || a == b ) return 0;
    if( !a ) return -1;
    if( !b ) return  1;

    for( std::size_t i = 0; i < n; ++i )
    {
        const int ca = ascii_lower( static_cast<unsigned char>( a[i] ) );
        const int cb = ascii_lower( static_cast<unsigned char>( b[i] ) );
        if( ca != cb ) return ca - cb;
        if( ca == 0  ) return 0;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// snprintf / vsnprintf
// ---------------------------------------------------------------------------

int vsnprintf( char *buf, std::size_t size, const char *fmt, std::va_list args ) noexcept
{
    if( !buf || !size || !fmt ) return 0;

    const int ret = std::vsnprintf( buf, size, fmt, args );
    buf[size - 1] = '\0'; // guarantee termination on truncation

    // Return bytes actually written (not counting \0), clamped.
    if( ret < 0 ) return 0;
    return ret < static_cast<int>( size ) ? ret : static_cast<int>( size ) - 1;
}

int snprintf( char *buf, std::size_t size, const char *fmt, ... ) noexcept
{
    std::va_list args;
    va_start( args, fmt );
    const int ret = vsnprintf( buf, size, fmt, args );
    va_end( args );
    return ret;
}

// ---------------------------------------------------------------------------
// atoi / atof / atov
// ---------------------------------------------------------------------------

static int parse_hex( int sign, std::string_view s ) noexcept
{
    if( s.size() >= 2 && s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) )
        s.remove_prefix( 2 );

    int val = 0;
    for( const char c : s )
    {
        if     ( c >= '0' && c <= '9' ) val = ( val << 4 ) + c - '0';
        else if( c >= 'a' && c <= 'f' ) val = ( val << 4 ) + c - 'a' + 10;
        else if( c >= 'A' && c <= 'F' ) val = ( val << 4 ) + c - 'A' + 10;
        else break;
    }
    return sign * val;
}

int atoi( std::string_view s ) noexcept
{
    s = skip_spaces( s );
    if( s.empty() ) return 0;

    int sign = 1;
    if( s.front() == '-' ) { sign = -1; s.remove_prefix( 1 ); }

    if( s.size() >= 2 && s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) )
        return parse_hex( sign, s );
    if( !s.empty() && s.front() == '\'' )
        return sign * static_cast<unsigned char>( s.size() >= 2 ? s[1] : 0 );

    int val = 0;
    for( const char c : s )
    {
        if( c < '0' || c > '9' ) break;
        val = val * 10 + ( c - '0' );
    }
    return val * sign;
}

float atof( std::string_view s ) noexcept
{
    s = skip_spaces( s );
    if( s.empty() ) return 0.0f;

    int sign = 1;
    if( s.front() == '-' ) { sign = -1; s.remove_prefix( 1 ); }

    if( s.size() >= 2 && s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) )
        return static_cast<float>( parse_hex( sign, s ) );
    if( !s.empty() && s.front() == '\'' )
        return static_cast<float>( sign * static_cast<unsigned char>( s.size() >= 2 ? s[1] : 0 ) );

    // Accumulate in double to match legacy precision.
    double val = 0.0;
    int decimal = -1, total = 0;
    for( const char c : s )
    {
        if( c == '.' ) { decimal = total; continue; }
        if( c < '0' || c > '9' ) break;
        val = val * 10.0 + static_cast<double>( c - '0' );
        ++total;
    }
    if( decimal >= 0 )
        while( total > decimal ) { val /= 10.0; --total; }

    return static_cast<float>( val * sign );
}

void atov( std::span<float> out, std::string_view s ) noexcept
{
    for( float &v : out ) v = 0.0f;
    for( float &v : out )
    {
        const auto start = s.find_first_not_of( ' ' );
        if( start == std::string_view::npos ) break;
        s.remove_prefix( start );
        v = atof( s );
        const auto next = s.find( ' ' );
        if( next == std::string_view::npos ) break;
        s.remove_prefix( next );
    }
}

// ---------------------------------------------------------------------------
// strip_colors  (strips ^N Quake-style color codes)
// ---------------------------------------------------------------------------

void strip_colors( const char *in, char *out ) noexcept
{
    if( !in || !out ) return;
    while( *in )
    {
        if( *in == '^' && *( in + 1 ) )
            in += 2;
        else
            *out++ = *in++;
    }
    *out = '\0';
}

// ---------------------------------------------------------------------------
// pretify_mem
// ---------------------------------------------------------------------------

std::string pretify_mem( float value, int decimals ) noexcept
{
    static constexpr float ONE_KB = 1024.0f;
    static constexpr float ONE_MB = ONE_KB * ONE_KB;

    const char *suffix;
    if( value > ONE_MB )      { value /= ONE_MB; suffix = "Mb"; }
    else if( value > ONE_KB ) { value /= ONE_KB; suffix = "Kb"; }
    else                      { suffix = "bytes"; }

    // Format the numeric portion only.
    char val[32];
    const bool is_integral = std::fabs( value - static_cast<float>( static_cast<int>( value ) ) )
                             < 0.00001f || decimals <= 0;
    if( is_integral )
        std::snprintf( val, sizeof val, "%d", static_cast<int>( value + 0.5f ) );
    else
        std::snprintf( val, sizeof val, "%.*f", decimals, static_cast<double>( value ) );

    // Walk val inserting thousands commas, then append " suffix".
    // pos starts 3 before the decimal (or end-of-integer) — inserts comma
    // whenever pos >= 0 && pos % 3 == 0, but never at the first character.
    const char *dot = std::strchr( val, '.' );
    if( !dot ) dot = val + std::strlen( val );
    int pos = static_cast<int>( dot - val ) - 3;

    std::string result;
    result.reserve( 32 );
    for( const char *i = val; *i; ++i, --pos )
    {
        if( pos >= 0 && !( pos % 3 ) && !result.empty() )
            result += ',';
        result += *i;
    }
    result += ' ';
    result += suffix;
    return result;
}

// ---------------------------------------------------------------------------
// match_pattern  — iterative/recursive wildcard glob
// ---------------------------------------------------------------------------

static bool match_pattern_impl( std::string_view text, std::string_view pattern,
                                 bool ci, std::string_view sep,
                                 bool wlo ) noexcept
{
    while( !pattern.empty() )
    {
        const char p = pattern.front();

        if( p == '?' )
        {
            if( text.empty() || ( !sep.empty() && sep.find( text.front() ) != std::string_view::npos ) )
                return false;
            text.remove_prefix( 1 );
            pattern.remove_prefix( 1 );
        }
        else if( p == '*' )
        {
            if( wlo )
            {
                if( text.empty() || ( !sep.empty() && sep.find( text.front() ) != std::string_view::npos ) )
                    return false;
                text.remove_prefix( 1 );
            }
            pattern.remove_prefix( 1 );
            // Try matching pattern at every remaining position in text.
            while( !text.empty() )
            {
                if( !sep.empty() && sep.find( text.front() ) != std::string_view::npos )
                    break;
                if( match_pattern_impl( text, pattern, ci, sep, wlo ) )
                    return true;
                text.remove_prefix( 1 );
            }
            // Let the outer loop check if remaining pattern matches empty text.
        }
        else
        {
            if( text.empty() ) return false;
            const int c1 = ci ? ascii_lower( static_cast<unsigned char>( text.front() ) )
                              :               static_cast<unsigned char>( text.front() );
            const int c2 = ci ? ascii_lower( static_cast<unsigned char>( p ) )
                              :               static_cast<unsigned char>( p );
            if( c1 != c2 ) return false;
            text.remove_prefix( 1 );
            pattern.remove_prefix( 1 );
        }
    }
    return text.empty();
}

bool match_pattern( std::string_view text, std::string_view pattern,
                    bool case_insensitive, std::string_view separators,
                    bool wildcard_least_one ) noexcept
{
    return match_pattern_impl( text, pattern, case_insensitive, separators, wildcard_least_one );
}

// ---------------------------------------------------------------------------
// parse_token  — single-step tokeniser (port of COM_ParseFileSafe)
// ---------------------------------------------------------------------------

static bool is_single_char( TokenFlags flags, char c ) noexcept
{
    using F = TokenFlags;
    switch( c )
    {
    case '{': case '}':
        return true;
    case ',':
        return ( flags & F::NoCommaAsToken ) == F::None;
    case '\'':
        return ( flags & F::NoSingleQuoteAsToken ) == F::None;
    case '(': case ')':
        return ( flags & F::NoBracketsAsToken ) == F::None;
    case ':':
        return ( flags & F::ColonAsToken ) != F::None;
    case '\n':
        return ( flags & F::NewlineAsToken ) != F::None;
    }
    return false;
}

const char *parse_token( const char *data, char *token, std::size_t token_size,
                         TokenFlags flags, int *out_len, bool *out_quoted ) noexcept
{
    using F = TokenFlags;

    if( out_quoted ) *out_quoted = false;

    if( !token || !token_size )
    {
        if( out_len ) *out_len = 0;
        return nullptr;
    }

    token[0] = '\0';
    if( !data ) return nullptr;

    std::size_t len = 0;
    bool overflow   = false;
    const bool nl_token = ( flags & F::NewlineAsToken ) != F::None;
    const bool hash_cmt = ( flags & F::HashAsComment ) != F::None;

    // Skip whitespace and line comments, retrying after each comment.
    // The inner loop stops at: end-of-string, or newline when NewlineAsToken.
    for( ;; )
    {
        unsigned char c;
        while( ( c = static_cast<unsigned char>( *data ) ) != '\0'
               && c <= ' '
               && !( nl_token && c == '\n' ) )
        {
            ++data;
        }

        c = static_cast<unsigned char>( *data );
        if( c == '\0' )
        {
            if( out_len ) *out_len = 0;
            return nullptr;
        }

        if( ( c == '/' && data[1] == '/' ) || ( c == '#' && hash_cmt ) )
        {
            while( *data && *data != '\n' ) ++data;
            continue;
        }
        break;
    }

    {
        const unsigned char c = static_cast<unsigned char>( *data );

        // Quoted string — supports \" escape.
        if( c == '"' && ( flags & F::NoQuotedTokens ) == F::None )
        {
            if( out_quoted ) *out_quoted = true;
            ++data;

            while( true )
            {
                const unsigned char q = static_cast<unsigned char>( *data );
                if( !q )
                {
                    token[len] = '\0';
                    if( out_len ) *out_len = overflow ? -1 : static_cast<int>( len );
                    return data;
                }
                ++data;

                if( q == '\\' && *data == '"' )
                {
                    if( len + 1 < token_size ) token[len++] = *data;
                    else                       overflow = true;
                    ++data;
                    continue;
                }
                if( q == '\"' )
                {
                    token[len] = '\0';
                    if( out_len ) *out_len = overflow ? -1 : static_cast<int>( len );
                    return data;
                }
                if( len + 1 < token_size ) token[len++] = static_cast<char>( q );
                else                       overflow = true;
            }
        }

        // Single-character token (braces, comma, colon, etc.).
        if( is_single_char( flags, static_cast<char>( c ) ) )
        {
            if( token_size >= 2 )
            {
                token[len++] = static_cast<char>( c );
                token[len]   = '\0';
                if( out_len ) *out_len = static_cast<int>( len );
                return data + 1;
            }
            token[0] = '\0';
            if( out_len ) *out_len = 0;
            return data;
        }

        // Regular word — consume until whitespace or single-char delimiter.
        unsigned char wc = c;
        do
        {
            if( len + 1 < token_size ) token[len++] = static_cast<char>( wc );
            else                       overflow = true;

            ++data;
            wc = static_cast<unsigned char>( *data );
        }
        while( wc > ' ' && !is_single_char( flags, static_cast<char>( wc ) ) );

        token[len] = '\0';
        if( out_len ) *out_len = overflow ? -1 : static_cast<int>( len );
        return data;
    }
}

// ---------------------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------------------

Tokenizer::Tokenizer( const char *data, TokenFlags flags ) noexcept
    : cursor_( data ), flags_( flags )
{
}

std::optional<Tokenizer::Token> Tokenizer::next() noexcept
{
    if( !cursor_ )
        return std::nullopt;

    int  len    = 0;
    bool quoted = false;

    cursor_ = parse_token( cursor_, buf_.data(), MAX_TOKEN, flags_, &len, &quoted );

    // parse_token returns nullptr (and len==0) when it reaches end of input.
    if( len == 0 && !quoted )
        return std::nullopt;

    return Token{ std::string_view{ buf_.data(), static_cast<std::size_t>( len ) }, quoted };
}

void Tokenizer::reset( const char *data ) noexcept
{
    cursor_ = data;
    buf_[0] = '\0';
}

// ---------------------------------------------------------------------------
// std::string strip_colors overload
// ---------------------------------------------------------------------------

std::string strip_colors( std::string_view in ) noexcept
{
    std::string out;
    out.reserve( in.size() );
    for( std::size_t i = 0; i < in.size(); )
    {
        if( in[i] == '^' && i + 1 < in.size() )
            i += 2;
        else
            out += in[i++];
    }
    return out;
}

} // namespace xash::utilities
