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

#include <xash3dpp/abi/abi_types.hpp>  // color24
#include <xash3dpp/abi/eiface.hpp>     // TYPEDESCRIPTION, FIELDTYPE, LEVELLIST, ENTITYTABLE, k_max_level_connections
#include <xash3dpp/abi/entity_state.hpp> // entity_state_t (STATICENTITY block)
#include <xash3dpp/limits.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace xash::save {

// Save directory (sv_save.c com_strings.h:59 DEFAULT_SAVE_DIRECTORY).  Every
// save-owned path (.sav / .HL1-3 / the *.HL? glob) is rooted here; save-
// directory ops and the file-backed I/O wrappers build paths against it.
inline constexpr std::string_view k_default_save_directory = "save/";

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

// ---------------------------------------------------------------------------
// .HL2 client-state structs (Chunk 8, slice S8.5).
// sv_save.c:64-83 (SAVE_CLIENT), :139-227 (gSaveClient/gDecalEntry/gStaticEntry/
// gSoundEntry descriptor tables).  save does not own the renderer's decallist_t
// (common/render_api.h) or the sound engine's soundlist_t (engine/common/
// common.h) — both are sibling-scope (render/sound), reached only through the
// OQ-4 capability interfaces (client_state.hpp).  SaveDecalEntry/SaveSoundEntry
// are save-owned WIRE-SHAPE mirrors of those structs (field-for-field, matching
// what gDecalEntry/gSoundEntry actually serialize) so this component never
// depends on renderer/sound headers.  entity_state_t IS already vendored
// (abi/entity_state.hpp, frozen game-DLL ABI) and is reused directly for
// STATICENTITY — gStaticEntry addresses its real member offsets.
// ---------------------------------------------------------------------------

// SAVE_CLIENT — root of the .HL2 body (sv_save.c:64-76).
struct SaveClient
{
    std::int32_t decal_count      = 0;
    std::int32_t entity_count     = 0;
    std::int32_t sound_count      = 0;
    std::int32_t temp_ents_count  = 0; // unused (legacy comment: "not used")
    char         intro_track[64]  = {};
    char         main_track[64]   = {};
    std::int32_t track_position   = 0;
    // Xash3D addition; serialized via FIELD_CHARACTER sizeof(short) (2 raw
    // bytes), NOT FIELD_SHORT — legacy comment: "mods based on HLU SDK
    // disallow usage of FIELD_SHORT" (format-level quirk, save-boundary.md).
    short        viewentity       = 0;
    float        wateralpha       = 0.0f;
    float        wateramp         = 0.0f;
};

inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 10> k_save_client_desc = { {
    define_field( ::xash::abi::FIELD_INTEGER,   "decalCount",     static_cast<int>( offsetof( SaveClient, decal_count ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "entityCount",    static_cast<int>( offsetof( SaveClient, entity_count ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "soundCount",     static_cast<int>( offsetof( SaveClient, sound_count ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "tempEntsCount",  static_cast<int>( offsetof( SaveClient, temp_ents_count ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "introTrack",     static_cast<int>( offsetof( SaveClient, intro_track ) ), 64 ),
    define_array( ::xash::abi::FIELD_CHARACTER, "mainTrack",      static_cast<int>( offsetof( SaveClient, main_track ) ), 64 ),
    define_field( ::xash::abi::FIELD_INTEGER,   "trackPosition",  static_cast<int>( offsetof( SaveClient, track_position ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "viewentity",     static_cast<int>( offsetof( SaveClient, viewentity ) ), sizeof( short ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "wateralpha",     static_cast<int>( offsetof( SaveClient, wateralpha ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "wateramp",       static_cast<int>( offsetof( SaveClient, wateramp ) ) ),
} };

// SaveDecalEntry — save-owned mirror of decallist_t (common/render_api.h) via
// gDecalEntry (sv_save.c:154-164).  `studio_state` is carried as OPAQUE bytes
// (30, sizeof(modelstate_t) — common/render_api.h "30 bytes here"): save does
// not need to interpret a studio decal's model-state, only preserve it
// byte-for-byte through the IDecalListProvider seam (client_state.hpp).
struct SaveDecalEntry
{
    float                     position[3]              = {};
    char                      name[64]                 = {};
    short                     entity_index              = 0; // FIELD_CHARACTER sizeof(short) quirk
    unsigned char             depth                     = 0;
    unsigned char             flags                     = 0;
    float                     scale                     = 0.0f;
    float                     impact_plane_normal[3]    = {};
    std::array<std::byte, 30> studio_state              = {}; // modelstate_t verbatim bytes
};

inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 8> k_decal_entry_desc = { {
    define_field( ::xash::abi::FIELD_VECTOR,    "position",           static_cast<int>( offsetof( SaveDecalEntry, position ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "name",               static_cast<int>( offsetof( SaveDecalEntry, name ) ), 64 ),
    define_array( ::xash::abi::FIELD_CHARACTER, "entityIndex",        static_cast<int>( offsetof( SaveDecalEntry, entity_index ) ), sizeof( short ) ),
    define_field( ::xash::abi::FIELD_CHARACTER, "depth",              static_cast<int>( offsetof( SaveDecalEntry, depth ) ) ),
    define_field( ::xash::abi::FIELD_CHARACTER, "flags",              static_cast<int>( offsetof( SaveDecalEntry, flags ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "scale",              static_cast<int>( offsetof( SaveDecalEntry, scale ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "impactPlaneNormal",  static_cast<int>( offsetof( SaveDecalEntry, impact_plane_normal ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "studio_state",       static_cast<int>( offsetof( SaveDecalEntry, studio_state ) ), 30 ),
} };

// SaveSoundEntry — save-owned mirror of soundlist_t (engine/common/common.h)
// via gSoundEntry (sv_save.c:216-227).
struct SaveSoundEntry
{
    char          name[64]        = {}; // MAX_QPATH == 64 (map_qpath_max)
    short         entnum          = 0;  // FIELD_CHARACTER sizeof(short) quirk
    float         origin[3]       = {};
    float         volume          = 0.0f;
    float         attenuation     = 0.0f;
    std::int32_t  looping         = 0;  // qboolean -> FIELD_BOOLEAN
    unsigned char channel         = 0;
    unsigned char pitch           = 0;
    unsigned char word_index      = 0;
    double        sample_pos      = 0.0;
    double        forced_end      = 0.0;
};

inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 11> k_sound_entry_desc = { {
    define_array( ::xash::abi::FIELD_CHARACTER, "name",        static_cast<int>( offsetof( SaveSoundEntry, name ) ), 64 ),
    define_array( ::xash::abi::FIELD_CHARACTER, "entnum",      static_cast<int>( offsetof( SaveSoundEntry, entnum ) ), sizeof( short ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "origin",      static_cast<int>( offsetof( SaveSoundEntry, origin ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "volume",      static_cast<int>( offsetof( SaveSoundEntry, volume ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "attenuation", static_cast<int>( offsetof( SaveSoundEntry, attenuation ) ) ),
    define_field( ::xash::abi::FIELD_BOOLEAN,   "looping",     static_cast<int>( offsetof( SaveSoundEntry, looping ) ) ),
    define_field( ::xash::abi::FIELD_CHARACTER, "channel",     static_cast<int>( offsetof( SaveSoundEntry, channel ) ) ),
    define_field( ::xash::abi::FIELD_CHARACTER, "pitch",       static_cast<int>( offsetof( SaveSoundEntry, pitch ) ) ),
    define_field( ::xash::abi::FIELD_CHARACTER, "wordIndex",   static_cast<int>( offsetof( SaveSoundEntry, word_index ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "samplePos",   static_cast<int>( offsetof( SaveSoundEntry, sample_pos ) ), sizeof( double ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "forcedEnd",   static_cast<int>( offsetof( SaveSoundEntry, forced_end ) ), sizeof( double ) ),
} };

// gStaticEntry — over the vendored abi::entity_state_t (frozen game-DLL ABI,
// abi/entity_state.hpp).  35 fields, sv_save.c:166-200; `controller`/`blending`
// are DEFINE_FIELD (not DEFINE_ARRAY) over `byte[4]` members — a 4-byte raw
// copy of the whole array read as one FIELD_INTEGER, matching legacy exactly
// (a real, preserved legacy quirk, not a bug introduced here).  `messagenum`
// (FIELD_MODELNAME) is a companion-TEXT field, not a raw copy — CORRECTED
// 2026-07-19 (S8.5 parity audit): the wire payload is the resolved model-name
// TEXT (strlen+1), matching how the ENGINE STRING family is actually written
// (eiface.h:366-367; the game-DLL codec routes MODELNAME/SOUNDNAME/STRING
// through WriteString), not the raw in-struct `string_t`/int handle, which is
// process-local and meaningless across processes.  See descriptor_codec.hpp
// FieldTextBinding and client_state.hpp's StaticEntityEntry::model_name.
inline constexpr std::array<::xash::abi::TYPEDESCRIPTION, 35> k_static_entry_desc = { {
    define_field( ::xash::abi::FIELD_MODELNAME, "messagenum", static_cast<int>( offsetof( ::xash::abi::entity_state_t, messagenum ) ) ), // HACKHACK: model stored in messagenum; TEXT via FieldTextBinding, see above
    define_field( ::xash::abi::FIELD_VECTOR,    "origin",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, origin ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "angles",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, angles ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "sequence",   static_cast<int>( offsetof( ::xash::abi::entity_state_t, sequence ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "frame",      static_cast<int>( offsetof( ::xash::abi::entity_state_t, frame ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "colormap",   static_cast<int>( offsetof( ::xash::abi::entity_state_t, colormap ) ) ),
    define_field( ::xash::abi::FIELD_SHORT,     "skin",       static_cast<int>( offsetof( ::xash::abi::entity_state_t, skin ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "body",       static_cast<int>( offsetof( ::xash::abi::entity_state_t, body ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "scale",      static_cast<int>( offsetof( ::xash::abi::entity_state_t, scale ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "effects",    static_cast<int>( offsetof( ::xash::abi::entity_state_t, effects ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "framerate",  static_cast<int>( offsetof( ::xash::abi::entity_state_t, framerate ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "mins",       static_cast<int>( offsetof( ::xash::abi::entity_state_t, mins ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "maxs",       static_cast<int>( offsetof( ::xash::abi::entity_state_t, maxs ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "startpos",   static_cast<int>( offsetof( ::xash::abi::entity_state_t, startpos ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "rendermode", static_cast<int>( offsetof( ::xash::abi::entity_state_t, rendermode ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "renderamt",  static_cast<int>( offsetof( ::xash::abi::entity_state_t, renderamt ) ) ),
    define_array( ::xash::abi::FIELD_CHARACTER, "rendercolor",static_cast<int>( offsetof( ::xash::abi::entity_state_t, rendercolor ) ), sizeof( ::xash::abi::color24 ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "renderfx",   static_cast<int>( offsetof( ::xash::abi::entity_state_t, renderfx ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "controller", static_cast<int>( offsetof( ::xash::abi::entity_state_t, controller ) ) ), // byte[4] read as one int (legacy quirk)
    define_field( ::xash::abi::FIELD_INTEGER,   "blending",   static_cast<int>( offsetof( ::xash::abi::entity_state_t, blending ) ) ),   // byte[4] read as one int (legacy quirk)
    define_field( ::xash::abi::FIELD_SHORT,     "solid",      static_cast<int>( offsetof( ::xash::abi::entity_state_t, solid ) ) ),
    define_field( ::xash::abi::FIELD_TIME,      "animtime",   static_cast<int>( offsetof( ::xash::abi::entity_state_t, animtime ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "movetype",   static_cast<int>( offsetof( ::xash::abi::entity_state_t, movetype ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "vuser1",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, vuser1 ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "vuser2",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, vuser2 ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "vuser3",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, vuser3 ) ) ),
    define_field( ::xash::abi::FIELD_VECTOR,    "vuser4",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, vuser4 ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "iuser1",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, iuser1 ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "iuser2",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, iuser2 ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "iuser3",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, iuser3 ) ) ),
    define_field( ::xash::abi::FIELD_INTEGER,   "iuser4",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, iuser4 ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "fuser1",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, fuser1 ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "fuser2",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, fuser2 ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "fuser3",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, fuser3 ) ) ),
    define_field( ::xash::abi::FIELD_FLOAT,     "fuser4",     static_cast<int>( offsetof( ::xash::abi::entity_state_t, fuser4 ) ) ),
} };

// ---------------------------------------------------------------------------
// SAV-OQ-1 — reserved embedded-file extension door (Chunk 8, slice S8.5).
// save-boundary.md "SAV-OQ-1 — Format extension door": `.HLX` reserves a
// third-character-wildcard-safe extension (matches the existing `*.HL?` glob
// used by ClearSaveDir / SaveGameSlot's DirectoryCopy scope) for a FUTURE
// xash3dpp-only embedded side-block.  NO PRODUCER SHIPS HERE (door-keep only,
// per the OQ's "Recommended shape") — this is the self-describing
// magic+version+size header shape a future side-block would use so a reader
// can skip an unrecognized block cleanly, plus the extension string.  The
// container reader (container_codec.hpp) tolerates an embedded `.HLX` record
// today for free: DirectoryExtract-equivalent extraction is already
// extension-blind (every embedded record round-trips regardless of name) —
// see the SAV-OQ-1 foreign-block round-trip test in tests/save.
// ---------------------------------------------------------------------------

inline constexpr std::string_view k_hlx_extension = ".HLX";

// A future .HLX side-block's own self-describing header (NOT part of the
// outer container-record framing — this is content INSIDE one embedded
// record's `data[fileSize]`, mirroring how GAME_HEADER/SAVE_HEADER are
// self-describing via the block-header record).  magic/version let an aware
// future reader validate/skip; `size` is the payload byte count following
// this 12-byte header, letting even an UNAWARE-of-the-specific-version reader
// skip the whole block by `size` bytes.  No xash3dpp-specific side-block
// content is designed here (door-keep only, per SAV-OQ-1).
struct HlxSideBlockHeader
{
    std::int32_t magic   = 0;
    std::int32_t version = 0;
    std::int32_t size    = 0; // payload bytes following this header
};
static_assert( sizeof( HlxSideBlockHeader ) == 12 );

// Reserved magic for a future .HLX side-block ('X'<<24|'L'<<16|'H'<<8|'X' —
// the same hand-rolled-int derivation style as k_savefile_magic/k_savegame_
// magic; chosen distinct from both so a reader can tell a .HLX side-block
// apart from a misnamed .HL1/.HL2).  version 1 is the first (unused) shape.
inline constexpr std::int32_t k_hlx_side_block_magic =
    ( std::int32_t{ 'X' } << 24 ) | ( std::int32_t{ 'L' } << 16 ) |
    ( std::int32_t{ 'H' } <<  8 ) |   std::int32_t{ 'X' };
inline constexpr std::int32_t k_hlx_side_block_version = 1;

} // namespace xash::save
