// xash3dpp — UTF-8/16 conversion implementation
// Legacy reference: public/utflib.c
//
// Codepage tables (CP1251/CP1252) live in this file; do not inline into header.

#include <xash3dpp/utilities/utf.hpp>
#include <cstring>

namespace xash::utilities::utf {

// ---------------------------------------------------------------------------
// Streaming UTF-8 decoder
//
// DecodeState fields used here:
//   codepoint  — accumulated codepoint bits so far
//   remaining  — total continuation bytes expected for this sequence (1–3)
//   accumulator — continuation bytes received so far
// ---------------------------------------------------------------------------

uint32_t decode_utf8( DecodeState &s, uint32_t byte ) noexcept
{
    if( s.remaining == 0 )
    {
        s.codepoint   = 0;
        s.accumulator = 0;

        if( byte <= 0x7fu )       return byte;  // ASCII fast path
        if( byte >= 0xf8u )       return 0;     // invalid lead byte

        if( byte >= 0xf0u )       { s.codepoint = byte & 0x07u; s.remaining = 3; }
        else if( byte >= 0xe0u )  { s.codepoint = byte & 0x0fu; s.remaining = 2; }
        else if( byte >= 0xc0u )  { s.codepoint = byte & 0x1fu; s.remaining = 1; }

        return 0;
    }

    // Expected a continuation byte (0x80–0xBF).
    if( byte > 0xbfu )
    {
        s.remaining = 0;
        return 0;
    }

    s.codepoint = ( s.codepoint << 6 ) | ( byte & 0x3fu );
    ++s.accumulator;

    if( s.accumulator == s.remaining )
    {
        s.remaining = 0;
        return s.codepoint;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Streaming UTF-16 decoder (surrogate pairs)
//
//   remaining  — 0: idle, 1: waiting for low surrogate
//   codepoint  — partial value built from the high surrogate
// ---------------------------------------------------------------------------

uint32_t decode_utf16( DecodeState &s, uint32_t unit ) noexcept
{
    if( s.remaining == 0 )
    {
        // BMP codepoint — no surrogate needed.
        if( unit < 0xd800u || unit > 0xdfffu )
            return unit;

        // High surrogate.
        s.codepoint = (( unit - 0xd800u ) << 10 ) + 0x10000u;
        s.remaining = 1;
        return 0;
    }

    // Expected low surrogate (0xDC00–0xDFFF).
    if( unit < 0xdc00u || unit > 0xdfffu )
    {
        s.remaining = 0;
        return 0;
    }

    const uint32_t cp = s.codepoint | ( unit - 0xdc00u );
    s.remaining = 0;
    return cp;
}

// ---------------------------------------------------------------------------
// UTF-8 encoder
// ---------------------------------------------------------------------------

static std::size_t codepoint_length( uint32_t cp ) noexcept
{
    if( cp <= 0x7fu   ) return 1;
    if( cp <= 0x7ffu  ) return 2;
    if( cp <= 0xffffu ) return 3;
    return 4;
}

std::pair<std::array<char, 4>, std::size_t> encode_utf8( uint32_t cp ) noexcept
{
    std::array<char, 4> dst{};
    switch( codepoint_length( cp ) )
    {
    case 1:
        dst[0] = static_cast<char>( cp );
        return { dst, 1 };
    case 2:
        dst[0] = static_cast<char>( 0xc0u | (( cp >>  6 ) & 0x1fu ) );
        dst[1] = static_cast<char>( 0x80u | (( cp       ) & 0x3fu ) );
        return { dst, 2 };
    case 3:
        dst[0] = static_cast<char>( 0xe0u | (( cp >> 12 ) & 0x0fu ) );
        dst[1] = static_cast<char>( 0x80u | (( cp >>  6 ) & 0x3fu ) );
        dst[2] = static_cast<char>( 0x80u | (( cp       ) & 0x3fu ) );
        return { dst, 3 };
    default:
        dst[0] = static_cast<char>( 0xf0u | (( cp >> 18 ) & 0x07u ) );
        dst[1] = static_cast<char>( 0x80u | (( cp >> 12 ) & 0x3fu ) );
        dst[2] = static_cast<char>( 0x80u | (( cp >>  6 ) & 0x3fu ) );
        dst[3] = static_cast<char>( 0x80u | (( cp       ) & 0x3fu ) );
        return { dst, 4 };
    }
}

// ---------------------------------------------------------------------------
// Bulk helpers
// ---------------------------------------------------------------------------

std::size_t length( std::string_view s ) noexcept
{
    DecodeState state{};
    std::size_t n = 0;
    for( const char c : s )
    {
        if( decode_utf8( state, static_cast<uint32_t>( static_cast<unsigned char>( c ) ) ) )
            ++n;
    }
    return n;
}

std::size_t utf16_to_utf8( std::span<char>                dst,
                            std::span<const std::uint16_t> src ) noexcept
{
    if( dst.empty() || src.empty() ) return 0;

    DecodeState state{};
    std::size_t out = 0;

    for( std::size_t i = 0; i < src.size() && src[i]; ++i )
    {
        const uint32_t cp = decode_utf16( state, src[i] );
        if( cp == 0 ) continue;

        const std::size_t enc = codepoint_length( cp );
        if( out + enc + 1 > dst.size() ) break;

        const auto [enc_buf, enc_len] = encode_utf8( cp );
        std::memcpy( dst.data() + out, enc_buf.data(), enc_len );
        out += enc_len;
    }

    dst[out] = '\0';
    return out;
}

// ---------------------------------------------------------------------------
// Codepage mappings
// ---------------------------------------------------------------------------

// Windows-1251 (Cyrillic) — bytes 0x80–0xBF map to this table.
static constexpr std::array<std::uint16_t, 64> k_cp1251_table = {{
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x007F, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
}};

uint32_t to_cp1251( uint32_t cp ) noexcept
{
    if( cp < 0x80u ) return cp;

    // Cyrillic capital letters А–Я (U+0410–U+042F) → 0xC0–0xDF
    if( cp >= 0x0410u && cp <= 0x042Fu ) return cp - 0x0410u + 0xC0u;
    // Cyrillic small letters а–я (U+0430–U+044F) → 0xE0–0xFF
    if( cp >= 0x0430u && cp <= 0x044Fu ) return cp - 0x0430u + 0xE0u;

    for( std::size_t i = 0; i < 64; ++i )
    {
        if( cp == static_cast<uint32_t>( k_cp1251_table[i] ) )
            return i + 0x80u;
    }

    return '?';
}

uint32_t to_cp1252( uint32_t cp ) noexcept
{
    // CP1252 is a strict superset of ISO-8859-1 for the 0x00–0xFF range.
    return cp < 0xFFu ? cp : '?';
}

// ---------------------------------------------------------------------------
// RAII decoder wrapper implementations
// ---------------------------------------------------------------------------

std::optional<uint32_t> Utf8Decoder::feed( uint8_t byte ) noexcept
{
    const uint32_t cp = decode_utf8( state_, byte );
    // remaining == 0 after the call means the sequence is complete (or invalid).
    // decode_utf8 returns the codepoint on completion — 0 is valid for U+0000.
    if( state_.remaining == 0 )
        return cp;
    return std::nullopt;
}

std::optional<uint32_t> Utf16Decoder::feed( uint16_t unit ) noexcept
{
    const uint32_t cp = decode_utf16( state_, unit );
    // remaining == 0: BMP codepoint emitted or surrogate pair completed.
    // remaining == 1: waiting for the low surrogate.
    if( state_.remaining == 0 )
        return cp;
    return std::nullopt;
}

} // namespace xash::utilities::utf
