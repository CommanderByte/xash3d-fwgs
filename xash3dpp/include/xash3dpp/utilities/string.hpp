#pragma once
// xash3dpp — string utilities
// Legacy reference: public/crtlib.h + public/crtlib.c
//
// Replaces Q_strncpy, Q_strlen, Q_strcmp/stricmp, Q_snprintf, Q_atoi/atof,
// COM_ParseFileSafe, COM_StripColors, matchpattern_with_separator, etc.

#include <xash3dpp/limits.hpp>

#include <array>
#include <cstddef>
#include <cstdarg>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xash::utilities {

// ---------------------------------------------------------------------------
// Safe bounded copy — always null-terminates dst.
// Legacy: Q_strncpy
char *strncpy( char *dst, const char *src, std::size_t size ) noexcept;

// Null-safe strlen — returns 0 for nullptr.
// Legacy: Q_strlen
[[nodiscard]] inline std::size_t strlen( const char *s ) noexcept
{
    if( !s ) return 0u;
    std::size_t n = 0;
    while( s[n] ) ++n;
    return n;
}

// Case-sensitive comparison; null-safe (nullptr orders before any string).
// Legacy: Q_strcmp
[[nodiscard]] int strcmp( const char *a, const char *b ) noexcept;

// Case-insensitive comparison.
// Legacy: Q_stricmp / Q_strnicmp
[[nodiscard]] int stricmp( const char *a, const char *b ) noexcept;
[[nodiscard]] int strnicmp( const char *a, const char *b, std::size_t n ) noexcept;

// Case-insensitive predicates for string_view pairs.
// Suitable as std::sort / std::lower_bound comparators.
// Subsumes private ci_less / ci_equal / iequal_sv helpers in the backends.
[[nodiscard]] inline bool ci_less( std::string_view a, std::string_view b ) noexcept
{
    const std::size_t n = ( a.size() > b.size() ? a.size() : b.size() ) + 1;
    return strnicmp( a.data(), b.data(), n ) < 0;
}
[[nodiscard]] inline bool ci_equal( std::string_view a, std::string_view b ) noexcept
{
    if ( a.size() != b.size() ) return false;
    return strnicmp( a.data(), b.data(), a.size() ) == 0;
}

// ASCII-only in-place and value-returning lowercase conversion.
inline void to_lower( std::string& s ) noexcept
{
    for( char& c : s )
        c = static_cast<char>( c >= 'A' && c <= 'Z' ? c + ( 'a' - 'A' ) : c );
}
[[nodiscard]] inline std::string to_lower( std::string_view s )
{
    std::string result( s );
    to_lower( result );
    return result;
}

// Strip leading and trailing characters found in 'chars' from sv.
// Default strip set is ASCII space and tab.
[[nodiscard]] inline std::string_view trim_sv( std::string_view sv,
                                  std::string_view chars = " \t" ) noexcept
{
    while ( !sv.empty() && chars.find( sv.front() ) != std::string_view::npos )
        sv.remove_prefix( 1 );
    while ( !sv.empty() && chars.find( sv.back() ) != std::string_view::npos )
        sv.remove_suffix( 1 );
    return sv;
}

// Bounded snprintf — always null-terminates; returns chars written (< size).
// Legacy: Q_snprintf / Q_vsnprintf
int snprintf( char *buf, std::size_t size, const char *fmt, ... ) noexcept;
int vsnprintf( char *buf, std::size_t size, const char *fmt, std::va_list args ) noexcept;

// Convert string to int / float / float[n].
// Legacy: Q_atoi, Q_atof, Q_atov
[[nodiscard]] int   atoi( std::string_view s ) noexcept;
[[nodiscard]] float atof( std::string_view s ) noexcept;
// Null-safe legacy overloads — preserve Q_atoi(NULL)==0 contract.
[[nodiscard]] inline int   atoi( const char *s ) noexcept { return atoi( s ? std::string_view{ s } : std::string_view{} ); }
[[nodiscard]] inline float atof( const char *s ) noexcept { return atof( s ? std::string_view{ s } : std::string_view{} ); }
void  atov( std::span<float> out, std::string_view s ) noexcept;

// Strip console color codes (^N sequences).
// Legacy: COM_StripColors
void        strip_colors( const char *in, char *out ) noexcept;
[[nodiscard]] std::string strip_colors( std::string_view in ) noexcept;

// Pretty-print a byte count ("1.5 MB").
// Legacy: Q_pretifymem
[[nodiscard]] std::string pretify_mem( float bytes, int decimals ) noexcept;

// Glob / wildcard pattern matching.
// Legacy: matchpattern_with_separator
[[nodiscard]] bool match_pattern( std::string_view text,
                    std::string_view pattern,
                    bool            case_insensitive,
                    std::string_view separators = {},
                    bool            wildcard_least_one = false ) noexcept;

// Tokeniser flags — mirror legacy PFILE_* values for compat.
enum class TokenFlags : unsigned
{
    None                  = 0,
    NoBracketsAsToken     = 1u << 0,
    ColonAsToken          = 1u << 1,
    HashAsComment         = 1u << 2,
    NoQuotedTokens        = 1u << 3,
    NoSingleQuoteAsToken  = 1u << 4,
    NoCommaAsToken        = 1u << 5,
    NewlineAsToken        = 1u << 6,
};

// Bitwise operators so flags can be composed without casts.
constexpr TokenFlags operator|( TokenFlags a, TokenFlags b ) noexcept
{
    return static_cast<TokenFlags>( static_cast<unsigned>( a ) | static_cast<unsigned>( b ) );
}
constexpr TokenFlags operator&( TokenFlags a, TokenFlags b ) noexcept
{
    return static_cast<TokenFlags>( static_cast<unsigned>( a ) & static_cast<unsigned>( b ) );
}
constexpr TokenFlags operator~( TokenFlags a ) noexcept
{
    return static_cast<TokenFlags>( ~static_cast<unsigned>( a ) );
}
constexpr TokenFlags &operator|=( TokenFlags &a, TokenFlags b ) noexcept { return a = a | b; }
constexpr TokenFlags &operator&=( TokenFlags &a, TokenFlags b ) noexcept { return a = a & b; }

// Single-token parser step.  Returns updated data pointer or nullptr at end.
// Legacy: COM_ParseFileSafe
[[nodiscard]] const char *parse_token( const char  *data,
                         char        *token,
                         std::size_t  token_size,
                         TokenFlags   flags     = TokenFlags::None,
                         int         *out_len   = nullptr,
                         bool        *out_quoted = nullptr ) noexcept;

// ---------------------------------------------------------------------------
// Tokenizer — stateful wrapper around parse_token.
//
// Eliminates threading the data pointer and flags through every call.
// The string_view inside Token is valid until the next call to next().
// ---------------------------------------------------------------------------

class Tokenizer
{
public:
    struct Token
    {
        std::string_view text;
        bool             quoted{};
    };

    static constexpr std::size_t MAX_TOKEN = xash::limits::tokenizer_token_max;

    explicit Tokenizer( const char *data,
                        TokenFlags  flags = TokenFlags::None ) noexcept;

    // Advance to the next token.  Returns nullopt at end of input.
    [[nodiscard]] std::optional<Token> next() noexcept;

    [[nodiscard]] bool        at_end() const noexcept { return cursor_ == nullptr; }
    void        reset( const char *data ) noexcept;

private:
    const char *cursor_{};
    TokenFlags  flags_{};
    std::array<char, MAX_TOKEN> buf_{};
};

} // namespace xash::utilities
