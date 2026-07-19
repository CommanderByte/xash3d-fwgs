#pragma once
// xash3dpp — save/restore on-disk format: magic/version tags, engine-owned
// header structs, and the TYPEDESCRIPTION field tables that drive them.
// Chunk 8, slice S8.1 (foundation).
//
// Legacy reference: engine/server/sv_save.c :31-37 (magics/versions/budgets),
// :40-83 (GAME_HEADER/SAVE_HEADER/SAVE_CLIENT/SAVE_LIGHTSTYLE structs), :91-137
// (gGameHeader/gSaveHeader/gAdjacency/gLightStyle/gEntityTable descriptor
// tables), engine/eiface.h :348-399 (FIELDTYPE / DEFINE_FIELD / TYPEDESCRIPTION).
// Byte layout cross-ref: docs/legacy-survey/deep-dive-server-save-boundary.md
// "Format internals (2026-07-19 recon)".
//
// This header is xash3dpp_save-private codec machinery (save-boundary.md
// §Satellite components) — it is layout/format only, no operations, no state.
// @thread-safety: pure constants + POD layout types; no shared state.
// @annotation-exempt: abi-pod — GameHeader/SaveHeader/SaveLightStyle are
// byte-exact mirrors of frozen on-disk records (field order == wire order); the
// descriptor tables are frozen ABI data.  Per-member @lifetime does not apply.

#include <xash3dpp/abi/eiface.hpp> // TYPEDESCRIPTION, FIELDTYPE, LEVELLIST, ENTITYTABLE, k_max_level_connections
#include <xash3dpp/limits.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace xash::save {

// ---------------------------------------------------------------------------
// Magic / version tags (sv_save.c:31-34)
// ---------------------------------------------------------------------------
//
// The legacy magics are hand-rolled ints, NOT C multichar literals, so their
// value is byte-order-independent at the source level; on disk the 4-byte int
// is written host-endian, and on the LE targets this fork supports the bytes
// spell the ASCII tag.  Derived here exactly as sv_save.c does, then pinned to
// the on-disk LE byte sequence by static_assert so the intent is explicit.
//   SAVEFILE_HEADER = ('V'<<24)|('L'<<16)|('A'<<8)|'V'  → 0x564C4156 → "VALV"
//   SAVEGAME_HEADER = ('V'<<24)|('A'<<16)|('S'<<8)|'J'  → 0x5641534A → "JSAV"
inline constexpr std::int32_t k_savefile_magic =
    ( std::int32_t{ 'V' } << 24 ) | ( std::int32_t{ 'L' } << 16 ) |
    ( std::int32_t{ 'A' } <<  8 ) |   std::int32_t{ 'V' };            // .HL1
inline constexpr std::int32_t k_savegame_magic =
    ( std::int32_t{ 'V' } << 24 ) | ( std::int32_t{ 'A' } << 16 ) |
    ( std::int32_t{ 'S' } <<  8 ) |   std::int32_t{ 'J' };            // .sav / .HL2

// LE on-disk byte order: writing the int little-endian yields the ASCII tag.
static_assert( k_savefile_magic == 0x564C4156, "VALV FOURCC" );
static_assert( k_savegame_magic == 0x5641534A, "JSAV FOURCC" );
static_assert( ( k_savefile_magic         & 0xFF ) == 'V' );  // byte 0 on disk
static_assert( ( ( k_savefile_magic >>  8 ) & 0xFF ) == 'A' ); // byte 1
static_assert( ( ( k_savefile_magic >> 16 ) & 0xFF ) == 'L' ); // byte 2
static_assert( ( ( k_savefile_magic >> 24 ) & 0xFF ) == 'V' ); // byte 3
static_assert( ( k_savegame_magic         & 0xFF ) == 'J' );  // byte 0 on disk
static_assert( ( ( k_savegame_magic >>  8 ) & 0xFF ) == 'S' ); // byte 1
static_assert( ( ( k_savegame_magic >> 16 ) & 0xFF ) == 'A' ); // byte 2
static_assert( ( ( k_savegame_magic >> 24 ) & 0xFF ) == 'V' ); // byte 3

// On-disk version tags (written as int; compared exact-match only, no tolerance
// band — sv_save.c:2317-2347).
inline constexpr std::int32_t k_savegame_version        = 0x0071; // SAVEGAME_VERSION (.sav/.HL1)
inline constexpr std::int32_t k_client_savegame_version = 0x0067; // CLIENT_SAVEGAME_VERSION (.HL2)

// Format budgets (see limits.hpp "save subsystem" block; re-exported here as the
// codec's own names so format code reads against these bounds directly).
inline constexpr std::size_t k_heap_size    = ::xash::limits::save_heap_size;    // 4 MiB
inline constexpr std::size_t k_hash_strings = ::xash::limits::save_hash_strings; // 4095
inline constexpr std::size_t k_max_level_connections =
    static_cast<std::size_t>( ::xash::abi::k_max_level_connections );            // 16 (ABI-frozen)

// ---------------------------------------------------------------------------
// Engine-owned header structs (field order == wire order; sv_save.c:40-83)
// ---------------------------------------------------------------------------
//
// C++ member names are snake_case per project convention, but the ON-DISK FIELD
// NAME (token-table-indexed via TYPEDESCRIPTION.fieldName below) is the frozen
// legacy C identifier — those strings are the wire format and are pinned as
// literals in the descriptor tables, decoupled from these member names.

// GAME_HEADER — root of the .sav container (sv_save.c:40-45).
struct GameHeader
{
    char         map_name[32];
    char         comment[80];
    std::int32_t map_count;
};
static_assert( sizeof( GameHeader ) == 116,               "GAME_HEADER wire size" );
static_assert( offsetof( GameHeader, map_name )  ==   0 );
static_assert( offsetof( GameHeader, comment )   ==  32 );
static_assert( offsetof( GameHeader, map_count ) == 112 );

// SAVE_HEADER — root of a .HL1 level file (sv_save.c:47-62); 13 fields.
struct SaveHeader
{
    std::int32_t skill_level;
    std::int32_t entity_count;
    std::int32_t connection_count;
    std::int32_t light_style_count;
    float        time; // FIELD_TIME
    char         map_name[32];
    char         sky_name[32];
    std::int32_t sky_color_r;
    std::int32_t sky_color_g;
    std::int32_t sky_color_b;
    float        sky_vec_x;
    float        sky_vec_y;
    float        sky_vec_z;
};
static_assert( sizeof( SaveHeader ) == 108,                    "SAVE_HEADER wire size" );
static_assert( offsetof( SaveHeader, skill_level )       ==  0 );
static_assert( offsetof( SaveHeader, entity_count )      ==  4 );
static_assert( offsetof( SaveHeader, connection_count )  ==  8 );
static_assert( offsetof( SaveHeader, light_style_count ) == 12 );
static_assert( offsetof( SaveHeader, time )              == 16 );
static_assert( offsetof( SaveHeader, map_name )          == 20 );
static_assert( offsetof( SaveHeader, sky_name )          == 52 );
static_assert( offsetof( SaveHeader, sky_color_r )       == 84 );
static_assert( offsetof( SaveHeader, sky_vec_x )         == 96 );

// SAVE_LIGHTSTYLE — repeated record (count = header.light_style_count),
// sv_save.c:78-83.
struct SaveLightStyle
{
    std::int32_t index;
    char         style[256];
    float        time;
};
static_assert( sizeof( SaveLightStyle ) == 264,          "SAVE_LIGHTSTYLE wire size" );
static_assert( offsetof( SaveLightStyle, index ) ==   0 );
static_assert( offsetof( SaveLightStyle, style ) ==   4 );
static_assert( offsetof( SaveLightStyle, time )  == 260 );

// ---------------------------------------------------------------------------
// TYPEDESCRIPTION builders (mirror eiface.h:380-382 DEFINE_FIELD / DEFINE_ARRAY)
// ---------------------------------------------------------------------------

// DEFINE_FIELD(type, name, ft) == { ft, "name", offsetof(type,name), 1, 0 }.
[[nodiscard]] constexpr ::xash::abi::TYPEDESCRIPTION
define_field( ::xash::abi::FIELDTYPE type, const char *wire_name, int offset ) noexcept
{
    return ::xash::abi::TYPEDESCRIPTION{ type, wire_name, offset, 1, 0 };
}

// DEFINE_ARRAY(type, name, ft, count) == { ft, "name", offsetof(type,name), count, 0 }.
[[nodiscard]] constexpr ::xash::abi::TYPEDESCRIPTION
define_array( ::xash::abi::FIELDTYPE type, const char *wire_name, int offset, short count ) noexcept
{
    return ::xash::abi::TYPEDESCRIPTION{ type, wire_name, offset, count, 0 };
}

// ---------------------------------------------------------------------------
// Engine-owned descriptor tables (sv_save.c:91-137).  fieldName == frozen wire
// token; fieldOffset == offsetof into the matching struct.  Offsets into the
// vendored ABI structs (LEVELLIST/ENTITYTABLE) are pointer-width-dependent and
// therefore per-arch — legacy computes them the same way at build time; the
// on-disk format never stores an offset (it locates fields in live memory), so
// per-arch offsets are correct within a build.
// ---------------------------------------------------------------------------

inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 3> k_game_header_desc = { {
    define_array( ::xash::abi::FIELD_CHARACTER, "mapName",  static_cast<int>( offsetof( GameHeader, map_name ) ), 32 ),
    define_array( ::xash::abi::FIELD_CHARACTER, "comment",  static_cast<int>( offsetof( GameHeader, comment ) ),  80 ),
    define_field( ::xash::abi::FIELD_INTEGER,   "mapCount", static_cast<int>( offsetof( GameHeader, map_count ) ) ),
} };

inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 13> k_save_header_desc = { {
    define_field( ::xash::abi::FIELD_INTEGER,   "skillLevel",      static_cast<int>( offsetof( SaveHeader, skill_level ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "entityCount",     static_cast<int>( offsetof( SaveHeader, entity_count ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "connectionCount", static_cast<int>( offsetof( SaveHeader, connection_count ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "lightStyleCount", static_cast<int>( offsetof( SaveHeader, light_style_count ) ) ),
    define_field( ::xash::abi::FIELD_TIME,      "time",            static_cast<int>( offsetof( SaveHeader, time ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "mapName",         static_cast<int>( offsetof( SaveHeader, map_name ) ), 32 ),
    define_array( ::xash::abi::FIELD_CHARACTER, "skyName",         static_cast<int>( offsetof( SaveHeader, sky_name ) ), 32 ),
    define_field( ::xash::abi::FIELD_INTEGER,   "skyColor_r",      static_cast<int>( offsetof( SaveHeader, sky_color_r ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "skyColor_g",      static_cast<int>( offsetof( SaveHeader, sky_color_g ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "skyColor_b",      static_cast<int>( offsetof( SaveHeader, sky_color_b ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "skyVec_x",        static_cast<int>( offsetof( SaveHeader, sky_vec_x ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "skyVec_y",        static_cast<int>( offsetof( SaveHeader, sky_vec_y ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "skyVec_z",        static_cast<int>( offsetof( SaveHeader, sky_vec_z ) ) ),
} };

// gAdjacency — over the vendored abi::LEVELLIST (its members already carry the
// frozen wire names).  sv_save.c:115-121.
inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 4> k_adjacency_desc = { {
    define_array( ::xash::abi::FIELD_CHARACTER, "mapName",           static_cast<int>( offsetof( ::xash::abi::LEVELLIST, mapName ) ), 32 ),
    define_array( ::xash::abi::FIELD_CHARACTER, "landmarkName",      static_cast<int>( offsetof( ::xash::abi::LEVELLIST, landmarkName ) ), 32 ),
    define_field( ::xash::abi::FIELD_EDICT,     "pentLandmark",      static_cast<int>( offsetof( ::xash::abi::LEVELLIST, pentLandmark ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "vecLandmarkOrigin", static_cast<int>( offsetof( ::xash::abi::LEVELLIST, vecLandmarkOrigin ) ) ),
} };

inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 3> k_light_style_desc = { {
    define_field( ::xash::abi::FIELD_INTEGER,   "index", static_cast<int>( offsetof( SaveLightStyle, index ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "style", static_cast<int>( offsetof( SaveLightStyle, style ) ), 256 ),
    define_field( ::xash::abi::FIELD_FLOAT,     "time",  static_cast<int>( offsetof( SaveLightStyle, time ) ) ),
} };

// gEntityTable — over the vendored abi::ENTITYTABLE.  5 of 6 members serialize:
// `pent` (the live edict_t*) is EXCLUDED from the wire format (sv_save.c:130-137,
// deep-dive "ENTITYTABLE serialization").
inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 5> k_entity_table_desc = { {
    define_field( ::xash::abi::FIELD_INTEGER, "id",        static_cast<int>( offsetof( ::xash::abi::ENTITYTABLE, id ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER, "location",  static_cast<int>( offsetof( ::xash::abi::ENTITYTABLE, location ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER, "size",      static_cast<int>( offsetof( ::xash::abi::ENTITYTABLE, size ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER, "flags",     static_cast<int>( offsetof( ::xash::abi::ENTITYTABLE, flags ) ) ),
    define_field( ::xash::abi::FIELD_STRING,  "classname", static_cast<int>( offsetof( ::xash::abi::ENTITYTABLE, classname ) ) ),
} };

} // namespace xash::save
