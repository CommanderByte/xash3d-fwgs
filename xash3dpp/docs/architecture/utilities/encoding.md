# Encoding utilities — UTF and byte-order swap

> **Headers**: `xash3dpp/include/xash3dpp/utilities/utf.hpp`,
> `xash3dpp/include/xash3dpp/utilities/swap.hpp`\
> **Sources**: `xash3dpp/src/utilities/utf.cpp`; `swap.hpp` is header-only\
> **Namespaces**: `xash::utilities::utf` (UTF functions), `xash::utilities` (swap)\
> **Legacy reference**: `public/utflib.h`, `public/utflib.c`, `public/swaplib.h`

## UTF encoding (utf.hpp / utf.cpp)

The UTF module provides streaming decode of UTF-8 and UTF-16 input, UTF-8
encoding, bulk conversion, and legacy Windows codepage mapping. All functions
are in namespace `xash::utilities::utf`.

### DecodeState

```cpp
struct DecodeState {
    uint32_t codepoint{};
    uint8_t  remaining{};
    uint8_t  accumulator{};
};
```

**Must be zero-initialised before first use.** The idiomatic form is
`DecodeState s{};` (value-initialisation). Passing an uninitialised
`DecodeState` to the decode functions is undefined behaviour.

The struct is small enough to sit on the stack. No heap allocation.

### Streaming UTF-8 decode

```cpp
uint32_t decode_utf8( DecodeState &s, uint32_t byte ) noexcept;
```

Feed one byte at a time. Returns 0 while the sequence is still incomplete,
and returns the decoded codepoint when the sequence finishes. The caller
repeats the loop until all input is consumed.

For invalid sequences the function resets `s` and returns 0, so subsequent
valid input resumes correctly. Callers that need to substitute U+FFFD for
invalid input should check for a zero return after all continuation bytes have
been fed.

### Streaming UTF-16 decode

```cpp
uint32_t decode_utf16( DecodeState &s, uint32_t unit ) noexcept;
```

Same contract as `decode_utf8` but accepts UTF-16 code units. Handles
surrogate pairs: returns 0 after the high surrogate and the codepoint after
the low surrogate arrives.

### UTF-8 encoding

```cpp
std::pair<std::array<char, 4>, std::size_t>
    encode_utf8( uint32_t codepoint ) noexcept;
```

Returns the encoded byte sequence and its length (1–4). The array is always
4 bytes; unused trailing bytes are zero. Codepoints beyond U+10FFFF are
replaced with U+FFFD.

### Bulk conversion

```cpp
// Number of Unicode codepoints in a UTF-8 string.
std::size_t length( std::string_view s ) noexcept;

// Convert UTF-16 to UTF-8; writes null-terminated result into dst.
// Returns the number of UTF-8 bytes written (excluding the null terminator).
std::size_t utf16_to_utf8( std::span<char>                dst,
                            std::span<const std::uint16_t> src ) noexcept;
```

`length` counts codepoints (not bytes), matching the legacy `utflib.c`
behaviour that the console uses to compute cursor positions.

`utf16_to_utf8` is used when reading filenames from Windows APIs and when
processing UTF-16 strings from external file formats (e.g., WAD file
comments).

### Legacy codepage mapping

```cpp
uint32_t to_cp1251( uint32_t codepoint ) noexcept;  // Unicode → Windows-1251
uint32_t to_cp1252( uint32_t codepoint ) noexcept;  // Unicode → Windows-1252
```

Map a Unicode codepoint to the nearest equivalent byte in the target codepage.
Used for the in-game console which renders glyphs from a 256-glyph font atlas
indexed by Windows-1251 or Windows-1252 code values. Codepoints with no
mapping return a fallback byte (typically `'?'` or `0x3F`).

### RAII decoder wrappers

```cpp
class Utf8Decoder {
public:
    std::optional<uint32_t> feed( uint8_t byte ) noexcept;
    void reset() noexcept;
};

class Utf16Decoder {
public:
    std::optional<uint32_t> feed( uint16_t unit ) noexcept;
    void reset() noexcept;
};
```

Both wrappers encapsulate a `DecodeState` member and expose a `feed` method:

- Returns `std::nullopt` while a multi-byte/surrogate sequence is incomplete.
- Returns `std::optional{codepoint}` when a sequence completes.
- Returns `std::optional{0}` for invalid sequences; callers may substitute
  U+FFFD.

The RAII wrappers eliminate the need to thread the `DecodeState` through
iteration code manually.

______________________________________________________________________

## Byte-order swap (swap.hpp, header-only)

> **Note**: `swap_struct` is declared but has no implementation yet. Only
> `swap_bytes` is callable. The `test_swap` test explicitly documents this
> limitation.

### `swap_bytes` (inline)

```cpp
inline void swap_bytes( void *p, std::size_t size ) noexcept;
```

Reverses the bytes of a 2-, 4-, or 8-byte value in place. A `switch` on `size`
selects the appropriate unrolled swap; any other size is a no-op.

Used at BSP/model load time to convert big-endian on-disk data to the native
little-endian format assumed by the rest of the engine.

Legacy: `BigShort`/`LittleShort`, `BigLong`/`LittleLong` macros from
`swaplib.h`.

### `SwapField` descriptor table

```cpp
struct SwapField {
    uint32_t         offset;   // byte offset of the field in the struct
    int32_t          size;     // field width in bytes (2, 4, or 8); negative → sub-struct
    const SwapField *subdef{}; // non-null when size < 0 (recursive table)
    uint32_t         count;    // array element count (1 for scalars)
    uint32_t         stride;   // element stride; 0 → use |size|
};

// Apply the descriptor table to a struct in-place (NOT YET IMPLEMENTED).
void swap_struct( void *data, std::span<const SwapField> fields ) noexcept;
```

`SwapField` tables describe the layout of a struct's multi-byte fields so that
`swap_struct` can apply `swap_bytes` to each one without requiring a
hand-written swap loop per struct type. This mirrors the `swap_struct_def_t`
approach from the legacy `swaplib.h`.

When `size < 0`, the field is treated as a nested struct whose own layout is
described by `subdef`. The `count` and `stride` fields handle packed arrays of
the same element type.

`swap_struct` has **no implementation** as of the current codebase. The
descriptor infrastructure is in place; the function body needs to be written
in a `swap.cpp` before use.

## Thread safety

All functions in this module operate on caller-provided state. There is no
shared mutable state. Separate `Utf8Decoder`/`Utf16Decoder` instances are
independent.
