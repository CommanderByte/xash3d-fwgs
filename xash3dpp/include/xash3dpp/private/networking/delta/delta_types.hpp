#pragma once
// xash3dpp — delta encoder internal types, flags, and wire constants
// Legacy reference: engine/common/net_encode.c (DT_* defines),
//                   engine/common/net_encode.h (goldsrc_delta_t, delta_info_t)
//
// Everything in this header is wire- or ABI-frozen unless noted.  The DT_*
// flag values travel inside svc_deltatable's 10-bit flags field and inside
// delta.lst scripts; the bit-width constants are the frozen field widths of
// the Xash delta protocol.  None of these are tunable capacities — they do
// NOT belong in limits.hpp (same rule as wire/wire_format.hpp magics).

#include <xash3dpp/limits.hpp>
#include <xash3dpp/networking/delta.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::networking::delta {

// ---------------------------------------------------------------------------
// Field-type flags (legacy DT_*) — wire-frozen bit values
// ---------------------------------------------------------------------------

inline constexpr std::uint32_t k_dt_byte           = 1u << 0;  // 1-byte field
inline constexpr std::uint32_t k_dt_short          = 1u << 1;  // 2-byte field
inline constexpr std::uint32_t k_dt_float          = 1u << 2;  // float scaled to integer
inline constexpr std::uint32_t k_dt_integer        = 1u << 3;  // 4-byte integer
inline constexpr std::uint32_t k_dt_angle          = 1u << 4;  // float angle, bit-angle codec
inline constexpr std::uint32_t k_dt_timewindow_8   = 1u << 5;  // timestamp rel. timebase, x100
inline constexpr std::uint32_t k_dt_timewindow_big = 1u << 6;  // timestamp rel. timebase, x multiplier
inline constexpr std::uint32_t k_dt_string         = 1u << 7;  // NUL-terminated string
inline constexpr std::uint32_t k_dt_signed         = 1u << 8;  // sign modificator
inline constexpr std::uint32_t k_dt_signed_gs      = 1u << 31; // GoldSrc sign bit (remapped on parse)

// Mask of the mutually-exclusive payload-type bits (excludes sign modifiers).
inline constexpr std::uint32_t k_dt_type_mask =
    k_dt_byte | k_dt_short | k_dt_float | k_dt_integer | k_dt_angle |
    k_dt_timewindow_8 | k_dt_timewindow_big | k_dt_string;

// ---------------------------------------------------------------------------
// Wire-frozen bit widths (Xash delta protocol)
// ---------------------------------------------------------------------------

inline constexpr int k_table_index_bits    = 4;  // svc_deltatable tableIndex
inline constexpr int k_name_index_bits     = 8;  // svc_deltatable nameIndex
inline constexpr int k_field_flags_bits    = 10; // svc_deltatable flags payload
inline constexpr int k_field_bits_bits     = 5;  // svc_deltatable (bits - 1) payload
inline constexpr int k_entity_number_bits  = 13; // legacy MAX_ENTITY_BITS (8192 edicts)
inline constexpr int k_entity_remove_bits  = 2;  // removeType: 1 = leave PVS, 2 = delete
inline constexpr int k_entity_baseline_bits = 7; // signed instanced-baseline index
inline constexpr int k_entity_type_bits    = 2;  // entityType payload when flagged
inline constexpr int k_gs_group_count_bits = 3;  // GoldSrc changed-byte-group count

// ---------------------------------------------------------------------------
// Legacy float helpers — parity-critical, DO NOT replace with std:: variants.
// Q_rint rounds half away from zero via +/-0.5 truncation (std::lrint uses
// banker's rounding); Q_equal is a +/-0.001f band, applied to multipliers to
// decide whether scaling happens at all.
// ---------------------------------------------------------------------------

inline constexpr float k_equal_epsilon = 0.001f; // legacy EQUAL_EPSILON

[[nodiscard]] constexpr bool q_equal( float a, float b ) noexcept
{
    return ( a >= ( b - k_equal_epsilon )) && ( a <= ( b + k_equal_epsilon ));
}

[[nodiscard]] constexpr int q_rint( double x ) noexcept
{
    return x < 0.0 ? static_cast<int>( x - 0.5 ) : static_cast<int>( x + 0.5 );
}

// float overload: legacy Q_rint is a macro, so float call sites round in
// float precision (TIMEWINDOW_BIG compare path computes `val * multiplier`
// in float).  Keeping both widths preserves that behaviour exactly.
[[nodiscard]] constexpr int q_rint( float x ) noexcept
{
    return x < 0.0f ? static_cast<int>( x - 0.5f ) : static_cast<int>( x + 0.5f );
}

// ---------------------------------------------------------------------------
// CustomEncodeKind — who owns a table's conditional-encode callback
// (legacy CUSTOM_NONE / CUSTOM_SERVER_ENCODE / CUSTOM_CLIENT_ENCODE)
// ---------------------------------------------------------------------------

enum class CustomEncodeKind : std::uint8_t
{
    None = 0,
    Server, // delta.lst "gamedll"
    Client, // delta.lst "clientdll"
};

// ---------------------------------------------------------------------------
// DeltaFieldInfo — compile-time field identity (legacy delta_field_t).
// Supplies name/offset/size; delta.lst or the wire supplies flags/bits/mults.
// ---------------------------------------------------------------------------

struct DeltaFieldInfo
{
    const char *name; // @lifetime: static string literal (compile-time field identity; immutable)
    int         offset;
    int         size;
};

// ---------------------------------------------------------------------------
// goldsrc_delta_t — GoldSrc wire meta-descriptor (svc_gs_deltadescription
// payload).  Layout mirrors the legacy struct exactly; parsed through the
// immutable meta-table in field_defs.hpp.
// ---------------------------------------------------------------------------

struct goldsrc_delta_t
{
    int   fieldType;
    char  fieldName[32];
    int   fieldOffset;
    short fieldSize;
    int   significant_bits;
    float premultiply;
    float postmultiply;
};

static_assert( sizeof( goldsrc_delta_t ) == 56 );
static_assert( offsetof( goldsrc_delta_t, fieldOffset )      == 36 );
static_assert( offsetof( goldsrc_delta_t, significant_bits ) == 44 ); // pad after short

// ---------------------------------------------------------------------------
// DeltaTable — one runtime table (legacy delta_info_t).
// Owned by DeltaTables; fields vector is sized once at init.
// ---------------------------------------------------------------------------

struct DeltaTable
{
    const char                    *name           { nullptr }; // static string
    std::span<const DeltaFieldInfo> info           {};          // compile-time identities
    std::vector<DeltaField>        fields          {};          // @pre-reserved: info.size() at init
    CustomEncodeKind               custom_encode   { CustomEncodeKind::None };
    // delta.lst encoder name (legacy funcName[32])
    char                           func_name[ ::xash::limits::net_delta_encoder_name ] {};
    DeltaEncodeFn                  user_callback   { nullptr };
    bool                           initialized     { false };
};

// The wire widths bound the table/field counts these limits document.
static_assert( static_cast<std::size_t>( DeltaStructId::Count )
               <= ::xash::limits::net_delta_max_tables );
static_assert( ::xash::limits::net_delta_max_tables <= ( 1u << k_table_index_bits ));
static_assert( ::xash::limits::net_delta_max_fields <= ( 1u << k_name_index_bits ));

} // namespace xash::networking::delta
