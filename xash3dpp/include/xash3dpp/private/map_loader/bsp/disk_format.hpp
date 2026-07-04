#pragma once
// xash3dpp — BSP on-disk format: lump directory, per-lump record layouts,
// version magics, and format capacity caps.
// Legacy reference: common/bspfile.h (all structs verbatim),
//                   common/wadfile.h:79-85 (mip_t),
//                   xash3dpp/docs/legacy-survey/deep-dive-bsp-loader.md
//
// Everything in this header is FORMAT-FROZEN: these are the byte layouts of
// BSP v29/v30/BSP2 files on disk.  Struct names deliberately keep the legacy
// d*_t spelling (same rule as include/xash3dpp/abi/ for frozen SDK PODs) so
// the 1:1 mapping to bspfile.h stays greppable.  The k_max_map_* caps are
// format facts, not tunable capacities — they do NOT belong in limits.hpp.
//
// The loader reads these by memcpy from the file image: little-endian only
// (matches every supported target; legacy swaps only under XASH_BIG_ENDIAN),
// natural alignment, no packing pragmas — verified by the static_asserts.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace xash::map_loader::bsp {

static_assert( std::endian::native == std::endian::little,
               "BSP disk structs are read by memcpy and assume a little-endian host" );

// ---------------------------------------------------------------------------
// Version magics (bspfile.h:31-43)
// ---------------------------------------------------------------------------

inline constexpr std::int32_t k_q1bsp_version  = 29;          // quake1 regular
inline constexpr std::int32_t k_hlbsp_version  = 30;          // half-life regular
inline constexpr std::int32_t k_qbsp2_version  =              // 'BSP2' fourcc
    ( 'B' ) | ( 'S' << 8 ) | ( 'P' << 16 ) | ( '2' << 24 );
inline constexpr std::int32_t k_extra_header_id =             // 'XASH' — BSP30ext
    ( 'X' ) | ( 'A' << 8 ) | ( 'S' << 16 ) | ( 'H' << 24 );
inline constexpr std::int32_t k_extra_version   = 4;          // current BSP30ext version

// ---------------------------------------------------------------------------
// Lump directory (bspfile.h:92-128)
// ---------------------------------------------------------------------------

inline constexpr int k_header_lumps = 15;
inline constexpr int k_extra_lumps  = 12;

inline constexpr int k_lump_entities     = 0;
inline constexpr int k_lump_planes       = 1;
inline constexpr int k_lump_textures     = 2;
inline constexpr int k_lump_vertexes     = 3;
inline constexpr int k_lump_visibility   = 4;
inline constexpr int k_lump_nodes        = 5;
inline constexpr int k_lump_texinfo      = 6;
inline constexpr int k_lump_faces        = 7;
inline constexpr int k_lump_lighting     = 8;
inline constexpr int k_lump_clipnodes    = 9;
inline constexpr int k_lump_leafs        = 10;
inline constexpr int k_lump_marksurfaces = 11;
inline constexpr int k_lump_edges        = 12;
inline constexpr int k_lump_surfedges    = 13;
inline constexpr int k_lump_models       = 14;

// ---------------------------------------------------------------------------
// Shared format facts
// ---------------------------------------------------------------------------

inline constexpr int k_max_map_hulls = 4;   // bspfile.h:50
inline constexpr int k_lm_styles     = 4;   // bspfile.h:62
inline constexpr int k_num_ambients  = 4;   // bspfile.h:140-147

// Plane types (public/xash3d_mathlib.h:65-68): 0/1/2 = axial X/Y/Z (fast
// path: PlaneDiff reads the coordinate directly), 3+ = non-axial dot product.
inline constexpr int k_plane_x        = 0;
inline constexpr int k_plane_y        = 1;
inline constexpr int k_plane_z        = 2;
inline constexpr int k_plane_nonaxial = 3;

// Texinfo flags (bspfile.h:130-135).
inline constexpr std::int16_t k_tex_special        = 1 << 0; // sky/slime: no lightmap, no 256 subdivision
inline constexpr std::int16_t k_tex_world_luxels   = 1 << 1;
inline constexpr std::int16_t k_tex_axial_luxels   = 1 << 2;
inline constexpr std::int16_t k_tex_extra_lightmap = 1 << 3;
inline constexpr std::int16_t k_tex_scroll         = 1 << 6; // Doom-style scrolling (conveyor)

// ---------------------------------------------------------------------------
// Format capacity caps (bspfile.h:67-89).  Enforcement is per-lump: lumps
// marked CHECK_OVERFLOW in the legacy srclumps[] table (nodes, texinfo,
// faces, leafs, models) abort on overflow, the rest only warn.
// ---------------------------------------------------------------------------

inline constexpr int k_max_map_clipnodes_hlbsp = 32767;      // 16-bit child cap
inline constexpr int k_max_map_clipnodes_bsp2  = 524288;
inline constexpr int k_max_map_models          = 2048;
inline constexpr int k_max_map_entstring       = 0x200000;   // 2 MB
inline constexpr int k_max_map_planes          = 131072;
inline constexpr int k_max_map_nodes           = 262144;
inline constexpr int k_max_map_leafs           = 131072;
inline constexpr int k_max_map_verts           = 524288;
inline constexpr int k_max_map_faces           = 262144;
inline constexpr int k_max_map_marksurfaces    = 524288;
inline constexpr int k_max_map_texinfo         = k_max_map_faces;
inline constexpr int k_max_map_edges           = 0x100000;
inline constexpr int k_max_map_surfedges       = 0x200000;
inline constexpr int k_max_map_textures        = 2048;
inline constexpr int k_max_map_miptex          = 0x2000000;  // 32 MB
inline constexpr int k_max_map_lighting        = 0x2000000;  // 32 MB
inline constexpr int k_max_map_visibility      = 0x1000000;  // 16 MB
inline constexpr int k_max_map_faceinfo        = 8192;

// ---------------------------------------------------------------------------
// Header directory (bspfile.h:152-183)
// ---------------------------------------------------------------------------

struct dlump_t
{
    std::int32_t fileofs;
    std::int32_t filelen;
};
static_assert( sizeof( dlump_t ) == 8 );

struct dheader_t
{
    std::int32_t version;
    dlump_t      lumps[k_header_lumps];
};
static_assert( sizeof( dheader_t ) == 124 );

// BSP30ext extra header, at file offset sizeof(dheader_t) == 124.
struct dextrahdr_t
{
    std::int32_t id;       // k_extra_header_id ('XASH')
    std::int32_t version;  // k_extra_version
    dlump_t      lumps[k_extra_lumps];
};
static_assert( sizeof( dextrahdr_t ) == 104 );

// ---------------------------------------------------------------------------
// Per-lump record layouts (bspfile.h:185-335).  16-bit classic form and
// *32_t BSP2 form where both exist.
// ---------------------------------------------------------------------------

struct dmodel_t
{
    float        mins[3];
    float        maxs[3];
    float        origin[3];                  // for sounds or lights
    std::int32_t headnode[k_max_map_hulls];
    std::int32_t visleafs;                   // not including the solid leaf 0
    std::int32_t firstface;
    std::int32_t numfaces;
};
static_assert( sizeof( dmodel_t ) == 64 );

// Start of LUMP_TEXTURES.  dataofs is declared [4] in the legacy header but
// is semantically [nummiptex]; -1 marks a miptex with no data.
struct dmiptexlump_t
{
    std::int32_t nummiptex;
    std::int32_t dataofs[4];
};
static_assert( sizeof( dmiptexlump_t ) == 20 );

// Miptex payload header (common/wadfile.h:79-85), pointed into by dataofs[i].
// offsets[0] > 0 → texels embedded in-BSP; <= 0 → name-only (external WAD).
struct mip_t
{
    char          name[16];
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t offsets[4];   // four mip levels
};
static_assert( sizeof( mip_t ) == 40 );

struct dvertex_t
{
    float point[3];
};
static_assert( sizeof( dvertex_t ) == 12 );

struct dplane_t
{
    float        normal[3];
    float        dist;
    std::int32_t type;          // k_plane_x .. k_plane_nonaxial+
};
static_assert( sizeof( dplane_t ) == 20 );

struct dnode_t
{
    std::int32_t  planenum;
    std::int16_t  children[2];  // negative numbers are -(leafs+1), not nodes
    std::int16_t  mins[3];
    std::int16_t  maxs[3];
    std::uint16_t firstface;
    std::uint16_t numfaces;     // counting both sides
};
static_assert( sizeof( dnode_t ) == 24 );
static_assert( offsetof( dnode_t, children ) == 4 );
static_assert( offsetof( dnode_t, firstface ) == 20 );

struct dnode32_t
{
    std::int32_t planenum;
    std::int32_t children[2];
    float        mins[3];
    float        maxs[3];
    std::int32_t firstface;
    std::int32_t numfaces;
};
static_assert( sizeof( dnode32_t ) == 44 );

// Leafs: leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid
// areas; it never has visibility data.
struct dleaf_t
{
    std::int32_t  contents;
    std::int32_t  visofs;                          // -1 = no visibility info
    std::int16_t  mins[3];
    std::int16_t  maxs[3];
    std::uint16_t firstmarksurface;
    std::uint16_t nummarksurfaces;
    std::uint8_t  ambient_level[k_num_ambients];
};
static_assert( sizeof( dleaf_t ) == 28 );
static_assert( offsetof( dleaf_t, firstmarksurface ) == 20 );

struct dleaf32_t
{
    std::int32_t contents;
    std::int32_t visofs;
    float        mins[3];
    float        maxs[3];
    std::int32_t firstmarksurface;
    std::int32_t nummarksurfaces;
    std::uint8_t ambient_level[k_num_ambients];
};
static_assert( sizeof( dleaf32_t ) == 44 );

struct dclipnode_t
{
    std::int32_t planenum;
    std::int16_t children[2];   // negative numbers are contents
};
static_assert( sizeof( dclipnode_t ) == 8 );

struct dclipnode32_t
{
    std::int32_t planenum;
    std::int32_t children[2];
};
static_assert( sizeof( dclipnode32_t ) == 12 );

struct dtexinfo_t
{
    float        vecs[2][4];    // texmatrix [s/t][xyz offset]
    std::int32_t miptex;
    std::int16_t flags;         // k_tex_* flags
    std::int16_t faceinfo;      // -1 = no, else index into dfaceinfo_t
};
static_assert( sizeof( dtexinfo_t ) == 40 );
static_assert( offsetof( dtexinfo_t, miptex ) == 32 );

// LUMP_FACEINFO (BSP30ext extra lump) — render-side; layout pinned for
// completeness of the format description.
struct dfaceinfo_t
{
    char          landname[16];
    std::uint16_t texture_step;
    std::uint16_t max_extent;
    std::int16_t  groupid;
};
static_assert( sizeof( dfaceinfo_t ) == 22 );

using dmarkface_t   = std::uint16_t;  // LUMP_MARKSURFACES, classic
using dmarkface32_t = std::int32_t;   // LUMP_MARKSURFACES, BSP2
using dsurfedge_t   = std::int32_t;   // LUMP_SURFEDGES

struct dedge_t
{
    std::uint16_t v[2];         // vertex numbers
};
static_assert( sizeof( dedge_t ) == 4 );

struct dedge32_t
{
    std::int32_t v[2];
};
static_assert( sizeof( dedge32_t ) == 8 );

struct dface_t
{
    std::uint16_t planenum;
    std::int16_t  side;
    std::int32_t  firstedge;    // int32 even in the classic form (>64k edges)
    std::int16_t  numedges;
    std::int16_t  texinfo;
    std::uint8_t  styles[k_lm_styles];
    std::int32_t  lightofs;     // byte offset into lightdata; -1 = none
};
static_assert( sizeof( dface_t ) == 20 );
static_assert( offsetof( dface_t, firstedge ) == 4 );
static_assert( offsetof( dface_t, lightofs ) == 16 );

struct dface32_t
{
    std::int32_t planenum;
    std::int32_t side;
    std::int32_t firstedge;
    std::int32_t numedges;
    std::int32_t texinfo;
    std::uint8_t styles[k_lm_styles];
    std::int32_t lightofs;
};
static_assert( sizeof( dface32_t ) == 28 );

// All disk records are read by memcpy from the file image.
static_assert( std::is_trivially_copyable_v<dheader_t> );
static_assert( std::is_trivially_copyable_v<dextrahdr_t> );
static_assert( std::is_trivially_copyable_v<dmodel_t> );
static_assert( std::is_trivially_copyable_v<dplane_t> );
static_assert( std::is_trivially_copyable_v<dnode_t> && std::is_trivially_copyable_v<dnode32_t> );
static_assert( std::is_trivially_copyable_v<dleaf_t> && std::is_trivially_copyable_v<dleaf32_t> );
static_assert( std::is_trivially_copyable_v<dclipnode_t> && std::is_trivially_copyable_v<dclipnode32_t> );
static_assert( std::is_trivially_copyable_v<dface_t> && std::is_trivially_copyable_v<dface32_t> );

} // namespace xash::map_loader::bsp
