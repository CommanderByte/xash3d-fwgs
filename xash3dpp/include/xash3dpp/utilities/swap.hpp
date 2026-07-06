#pragma once
// xash3dpp — byte-order swapping for on-disk structures
// Legacy reference: public/swaplib.h  (header-only in legacy too)
//
// Uses a reflection-style descriptor table so callers can describe struct
// field layouts without writing custom swap loops.
//
// @thread-safety: pure functions over caller-owned buffers — safe from any thread.

#include <bit>
#include <concepts>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>

namespace xash::utilities {

// ---------------------------------------------------------------------------
// Primitive byte-swap (compile-time width dispatch)
// ---------------------------------------------------------------------------

inline void swap_bytes( void *p, std::size_t size ) noexcept
{
    auto *b = static_cast<unsigned char *>( p );
    switch( size )
    {
    case 2: { unsigned char t = b[0]; b[0] = b[1]; b[1] = t; } break;
    case 4: { unsigned char t;
              t=b[0]; b[0]=b[3]; b[3]=t;
              t=b[1]; b[1]=b[2]; b[2]=t; } break;
    case 8: { unsigned char t;
              t=b[0]; b[0]=b[7]; b[7]=t;
              t=b[1]; b[1]=b[6]; b[6]=t;
              t=b[2]; b[2]=b[5]; b[5]=t;
              t=b[3]; b[3]=b[4]; b[4]=t; } break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Typed little-endian buffer read/write (modern binary-parser path)
// Legacy: LittleLong/LittleShort + memcpy, hand-written per field. Prefer these
// in new codecs/loaders — on little-endian targets the swap folds away at
// compile time (std::byteswap, C++23). Read floats as the integer of matching
// width then std::bit_cast at the call site.
// ---------------------------------------------------------------------------

// Unchecked: read a little-endian integer T from `src` (must hold >= sizeof(T)
// bytes). Use the span overload at parse boundaries where bounds are unknown.
template<std::integral T>
[[nodiscard]] T read_le( const std::byte *src ) noexcept
{
    T v;
    std::memcpy( &v, src, sizeof( T ) );
    if constexpr( std::endian::native == std::endian::big )
        v = std::byteswap( v );
    return v;
}

// Bounds-checked: read a little-endian integer T from `buf` at `offset`.
// Returns std::nullopt if the read would run past the end — the safety win for
// parsing untrusted files without hand-rolled size arithmetic (modernization H-4).
template<std::integral T>
[[nodiscard]] std::optional<T> read_le( std::span<const std::byte> buf, std::size_t offset ) noexcept
{
    if( offset > buf.size() || buf.size() - offset < sizeof( T ) )
        return std::nullopt;
    return read_le<T>( buf.data() + offset );
}

// Write a little-endian integer T to `dst` (must hold >= sizeof(T) bytes).
template<std::integral T>
void write_le( std::byte *dst, T v ) noexcept
{
    if constexpr( std::endian::native == std::endian::big )
        v = std::byteswap( v );
    std::memcpy( dst, &v, sizeof( T ) );
}

// ---------------------------------------------------------------------------
// Struct-descriptor-driven swap
// Legacy: swap_struct_def_t / SwapStruct in swaplib.h
// ---------------------------------------------------------------------------

struct SwapField
{
    uint32_t         offset;   // byte offset of field within struct
    int32_t          size;     // field size in bytes (2, 4, or 8); negative = sub-struct
    const SwapField *subdef{}; // non-null when size < 0 (recursive)
    uint32_t         count;    // number of elements (array field)
    uint32_t         stride;   // element stride (0 → use |size|)
};

// Apply the swap table to a struct in-place.
void swap_struct( void *data, std::span<const SwapField> fields ) noexcept;

} // namespace xash::utilities
