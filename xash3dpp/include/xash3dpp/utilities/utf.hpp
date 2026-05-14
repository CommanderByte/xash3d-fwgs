#pragma once
// xash3dpp — UTF-8/16 conversion
// Legacy reference: public/utflib.h + public/utflib.c
//
// IMPORTANT: utfstate_t must be zero-initialised before first use.
// Feed codepoints one byte at a time; function returns 0 while still decoding.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace xash::utilities::utf {

// ---------------------------------------------------------------------------
// Streaming decoders
// ---------------------------------------------------------------------------

struct DecodeState
{
    uint32_t codepoint{};
    uint8_t  remaining{};
    uint8_t  accumulator{};
};

// Feed one UTF-8 byte.  Returns decoded codepoint when complete, 0 otherwise.
uint32_t decode_utf8( DecodeState &s, uint32_t byte ) noexcept;

// Feed one UTF-16 code unit.  Returns decoded codepoint when complete, 0 otherwise.
uint32_t decode_utf16( DecodeState &s, uint32_t unit ) noexcept;

// ---------------------------------------------------------------------------
// Encoding
// ---------------------------------------------------------------------------

// Encode 'cp' as UTF-8.  Returns the encoded bytes and the byte count (1–4).
std::pair<std::array<char, 4>, std::size_t> encode_utf8( uint32_t codepoint ) noexcept;

// ---------------------------------------------------------------------------
// Bulk conversion
// ---------------------------------------------------------------------------

// Number of Unicode codepoints in a UTF-8 string.
std::size_t length( std::string_view s ) noexcept;

// Convert UTF-16 to UTF-8.  Writes null-terminated result into dst.
// Returns the number of UTF-8 bytes written (not counting the null terminator).
std::size_t utf16_to_utf8( std::span<char>                dst,
                            std::span<const std::uint16_t> src ) noexcept;

// ---------------------------------------------------------------------------
// Legacy codepage mappings (console/Windows compatibility)
// ---------------------------------------------------------------------------

// Map a Unicode codepoint to the nearest Windows-1251 byte (Cyrillic).
uint32_t to_cp1251( uint32_t codepoint ) noexcept;

// Map a Unicode codepoint to the nearest Windows-1252 byte (Latin-1 extended).
uint32_t to_cp1252( uint32_t codepoint ) noexcept;

// ---------------------------------------------------------------------------
// RAII decoder wrappers
//
// Returns nullopt while a multi-byte/surrogate sequence is still incomplete.
// Returns optional{codepoint} when a sequence finishes (including U+0000).
// Returns optional{0} for invalid byte sequences (caller may substitute U+FFFD).
// ---------------------------------------------------------------------------

class Utf8Decoder
{
public:
    std::optional<uint32_t> feed( uint8_t byte ) noexcept;
    void reset() noexcept { state_ = {}; }

private:
    DecodeState state_{};
};

class Utf16Decoder
{
public:
    std::optional<uint32_t> feed( uint16_t unit ) noexcept;
    void reset() noexcept { state_ = {}; }

private:
    DecodeState state_{};
};

} // namespace xash::utilities::utf
