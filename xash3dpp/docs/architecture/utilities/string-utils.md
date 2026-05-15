# String utilities

> **Headers**: `xash3dpp/include/xash3dpp/utilities/string.hpp`  
> **Source**: `xash3dpp/src/utilities/string.cpp`  
> **Namespace**: `xash::utilities`  
> **Legacy reference**: `public/crtlib.h`, `public/crtlib.c`

## Purpose

Replaces the flat collection of `Q_str*`, `Q_snprintf`, `COM_ParseFileSafe`,
`COM_StripColors`, `matchpattern_with_separator`, and `Q_pretifymem` functions
from `public/crtlib.c`. All functions are `noexcept` unless they allocate and
return a `std::string`.

## Bounded copy and comparison

```cpp
// Always null-terminates dst; legacy: Q_strncpy
char *strncpy( char *dst, const char *src, std::size_t size ) noexcept;

// Null-safe (returns 0 for nullptr); legacy: Q_strlen
std::size_t strlen( const char *s ) noexcept;   // inline

// Case-insensitive comparison; legacy: Q_stricmp, Q_strnicmp
int stricmp ( const char *a, const char *b ) noexcept;
int strnicmp( const char *a, const char *b, std::size_t n ) noexcept;

// string_view predicates — drop-in for std::sort / std::lower_bound
bool ci_less ( std::string_view a, std::string_view b ) noexcept;   // inline
bool ci_equal( std::string_view a, std::string_view b ) noexcept;   // inline
```

`strncpy` is guaranteed to write a null terminator even when `src` is longer
than `size - 1`. This is the one contract the standard `strncpy` does not give.

`strlen` is null-safe: passing `nullptr` returns 0 rather than crashing. This
preserves `Q_strlen(NULL) == 0` which legacy code relies upon.

## Case folding

```cpp
void        to_lower( std::string& s ) noexcept;        // in-place, inline
std::string to_lower( std::string_view s );             // value-returning
```

Both operate on ASCII characters only (`'A'`–`'Z'` → `'a'`–`'z'`). Multi-byte
or locale-aware folding is handled by `xash::utilities::utf` (see
[encoding.md](./encoding.md)).

## Formatted output

```cpp
int snprintf ( char *buf, std::size_t size, const char *fmt, ... ) noexcept;
int vsnprintf( char *buf, std::size_t size, const char *fmt, std::va_list ) noexcept;
```

Both always null-terminate `buf` and return the number of characters written
(always `< size`). The return value is never negative, unlike the C standard
`vsnprintf` which may return a negative value on encoding errors.

Legacy: `Q_snprintf` / `Q_vsnprintf`.

## Numeric conversion

```cpp
int   atoi( std::string_view s ) noexcept;
float atof( std::string_view s ) noexcept;

// Null-safe legacy overloads — preserve Q_atoi(NULL)==0
int   atoi( const char *s ) noexcept;   // inline
float atof( const char *s ) noexcept;   // inline

// Parse whitespace-separated floats into a span
void  atov( std::span<float> out, std::string_view s ) noexcept;
```

`atoi` recognises:
- Decimal integers (`"-42"`)
- Hexadecimal with `0x` prefix (`"0xFF"`)
- Single-character ASCII literals (`"'A'"` → 65)

`atof` accepts the same decimal/hex forms as `atoi` and standard
floating-point notation. Hex float literals (`0x1.8p+1`) are not supported.

`atov` parses at most `out.size()` floats from a space-separated string;
remaining elements are left unchanged. Used for parsing vectors from config.

## View helpers

```cpp
// Strip leading and trailing characters in `chars` from sv.
// Default strip set: space (0x20) and horizontal tab (0x09).
// Returns a view into the input — no allocation.
std::string_view trim_sv( std::string_view sv,
                          std::string_view chars = " \t" ) noexcept;  // inline
```

`trim_sv` is the `string_view`-native alternative to `Q_strtrim`-style helpers.
It does not modify any buffer. Contrast with `trim_space` in `path.hpp`, which
writes into a caller-provided char buffer.

## Color-code stripping

```cpp
void        strip_colors( const char *in, char *out ) noexcept;
std::string strip_colors( std::string_view in ) noexcept;
```

Removes Half-Life 1 `^N` console colour escape sequences (where N is any digit
0–9). The raw form writes into a caller-provided buffer of the same length as
`in`. The `std::string`-returning form is for config/UI code.

Legacy: `COM_StripColors`.

## Pretty-printing

```cpp
std::string pretify_mem( float bytes, int decimals ) noexcept;
```

Formats a byte count with a SI-style suffix (`B`, `KB`, `MB`, `GB`). Returns a
`std::string` such as `"1.5 MB"` or `"512 B"`. Decimals are clamped to 0–3.

Legacy: `Q_pretifymem`.

## Wildcard / glob matching

```cpp
bool match_pattern( std::string_view text,
                    std::string_view pattern,
                    bool             case_insensitive,
                    std::string_view separators = {},
                    bool             wildcard_least_one = false ) noexcept;
```

`*` matches any run of characters that does not contain a character from
`separators`. Setting `wildcard_least_one = true` makes `*` require at least one
character (non-zero-length match). `?` matches exactly one character.

Legacy: `matchpattern_with_separator`.

## Tokenizer

The tokenizer API consists of a low-level step function and a stateful
object wrapper.

### `parse_token` (low-level)

```cpp
enum class TokenFlags : unsigned { None, NoBracketsAsToken, ColonAsToken,
    HashAsComment, NoQuotedTokens, NoSingleQuoteAsToken, NoCommaAsToken,
    NewlineAsToken };

const char *parse_token( const char  *data,
                         char        *token,
                         std::size_t  token_size,
                         TokenFlags   flags     = TokenFlags::None,
                         int         *out_len   = nullptr,
                         bool        *out_quoted = nullptr ) noexcept;
```

Returns the updated cursor (the caller's new `data` pointer) or `nullptr` when
the input is exhausted. Writes into `token`, always null-terminating it.
`out_quoted` reports whether the token was enclosed in double quotes.

Legacy: `COM_ParseFileSafe`.

`TokenFlags` values mirror the legacy `PFILE_*` enum exactly so that code that
previously passed numeric flag constants will produce the same result.

### `Tokenizer` (stateful wrapper)

```cpp
class Tokenizer {
public:
    struct Token { std::string_view text; bool quoted{}; };
    static constexpr std::size_t MAX_TOKEN = xash::limits::tokenizer_token_max;

    explicit Tokenizer( const char *data,
                        TokenFlags  flags = TokenFlags::None ) noexcept;
    std::optional<Token> next() noexcept;
    bool at_end() const noexcept;
    void reset( const char *data ) noexcept;
};
```

Maintains the cursor and a `limits::tokenizer_token_max`-byte (512) stack buffer internally. `Token::text` is a
`string_view` into that buffer, valid until the next call to `next()`. Callers
that need to retain a token must copy it.

There is no heap allocation; the buffer is a `std::array<char, MAX_TOKEN>` member.

## Thread safety

All functions are stateless or operate on caller-provided state. `Tokenizer` is
not thread-safe for concurrent `next()` calls on the same object, but separate
instances are independent.
