#pragma once
// xash3dpp — byte-order swapping for on-disk structures
// Legacy reference: public/swaplib.h  (header-only in legacy too)
//
// Uses a reflection-style descriptor table so callers can describe struct
// field layouts without writing custom swap loops.

#include <cstdint>
#include <cstddef>
#include <cstring>
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
