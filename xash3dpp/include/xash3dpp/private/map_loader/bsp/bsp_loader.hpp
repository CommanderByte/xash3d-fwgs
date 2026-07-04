#pragma once
// xash3dpp — BSP loader pipeline internals, shared by the src/map_loader/bsp/
// TUs and their white-box tests.
// Legacy reference: engine/common/mod_bmodel.c — Mod_LoadBmodelLumps
// (version dispatch + quirk detection, :4242-4384), Mod_LoadLump (per-lump
// validation, :760-952), srclumps[] (validation table, :350-510),
// Mod_LumpLooksLikeEntities (:4064).
//
// Parity notes vs legacy, recorded as Known Deviations in the boundary doc:
//  - lump ranges are bounds-checked against the file image (legacy reads
//    out-of-buffer without validation) — wire-reachable hardening;
//  - a validation error in ANY required lump fails the load for world models
//    too (legacy counts errors but proceeds for the world — the
//    "a1ba: why world excluded here?" branch).

#include <xash3dpp/core/error.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/map_loader/bsp/disk_format.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace xash::map_loader::bsp {

// ---------------------------------------------------------------------------
// Per-lump validation metadata — legacy srclumps[] (mod_bmodel.c:350-510).
// mincount/maxcount are in ELEMENTS of the resolved entry size; lumps with a
// 32-bit record variant carry entrysize32 (0 = no variant).  check_overflow
// mirrors the legacy CHECK_OVERFLOW flag: over-maxcount is an error there
// and only a warning elsewhere.
// ---------------------------------------------------------------------------

struct LumpInfo
{
    int          lump;
    int          mincount;
    int          maxcount;
    std::size_t  entrysize;
    std::size_t  entrysize32;
    bool         check_overflow;
    const char  *name;
};

inline constexpr LumpInfo k_src_lumps[k_header_lumps] = {
    { k_lump_entities,     32, k_max_map_entstring,    1,                       0,                         false, "entities" },
    { k_lump_planes,        1, k_max_map_planes,       sizeof( dplane_t ),      0,                         false, "planes" },
    { k_lump_textures,      1, k_max_map_miptex,       1,                       0,                         false, "textures" },
    { k_lump_vertexes,      0, k_max_map_verts,        sizeof( dvertex_t ),     0,                         false, "vertexes" },
    { k_lump_visibility,    0, k_max_map_visibility,   1,                       0,                         false, "visibility" },
    { k_lump_nodes,         1, k_max_map_nodes,        sizeof( dnode_t ),       sizeof( dnode32_t ),       true,  "nodes" },
    { k_lump_texinfo,       0, k_max_map_texinfo,      sizeof( dtexinfo_t ),    0,                         true,  "texinfo" },
    { k_lump_faces,         0, k_max_map_faces,        sizeof( dface_t ),       sizeof( dface32_t ),       true,  "faces" },
    { k_lump_lighting,      0, k_max_map_lighting,     1,                       0,                         false, "lightmaps" },
    { k_lump_clipnodes,     0, k_max_map_clipnodes_bsp2, sizeof( dclipnode_t ), sizeof( dclipnode32_t ),   false, "clipnodes" },
    { k_lump_leafs,         1, k_max_map_leafs,        sizeof( dleaf_t ),       sizeof( dleaf32_t ),       true,  "leafs" },
    { k_lump_marksurfaces,  0, k_max_map_marksurfaces, sizeof( dmarkface_t ),   sizeof( dmarkface32_t ),   false, "markfaces" },
    { k_lump_edges,         0, k_max_map_edges,        sizeof( dedge_t ),       sizeof( dedge32_t ),       false, "edges" },
    { k_lump_surfedges,     0, k_max_map_surfedges,    sizeof( dsurfedge_t ),   0,                         false, "surfedges" },
    { k_lump_models,        1, k_max_map_models,       sizeof( dmodel_t ),      0,                         true,  "models" },
};

// ---------------------------------------------------------------------------
// Header / quirk detection — legacy Mod_LoadBmodelLumps version dispatch.
// ---------------------------------------------------------------------------

struct HeaderInfo
{
    ::xash::map_loader::BspVersion version = ::xash::map_loader::BspVersion::HalfLife;
    std::int32_t version_raw    = 0;
    bool         bsp30ext       = false; // 'XASH' id at file offset 124 (v30 only;
                                         // legacy checks only the id — NOT the extra
                                         // version — for the clipnode-guess flag)
    bool         blueshift_swap = false; // entities<->planes directory entries swapped
    bool         clipnodes32    = false; // clipnode records are dclipnode32_t
    dheader_t    header{};
};

// Validates size/version, detects BSP30ext, Blue-Shift swap and the extended
// clipnode format.  Errors: BspCorruptLump (short file),
// BspUnsupportedVersion (version not 29/30/'BSP2').
[[nodiscard]] std::expected<HeaderInfo, ::xash::core::ErrorCode>
parse_header( std::span<const std::byte> file ) noexcept;

// ---------------------------------------------------------------------------
// Lump resolution — legacy Mod_LoadLump validation, LOADLUMP_STANDARD path.
// ---------------------------------------------------------------------------

struct LumpView
{
    std::span<const std::byte> bytes{};    // raw record bytes (empty when absent)
    std::size_t                count = 0;  // elements at the resolved entry size
    std::size_t                entrysize = 0;
    bool                       present = false;
};

// Resolves one standard lump: applies the Blue-Shift entities/planes swap,
// picks the 16- vs 32-bit record size (BSP2 always 32; BSP30ext clipnode
// guess), and runs the legacy validation ladder (absent-if-fileofs-0,
// missing-required, size-multiple, mincount, maxcount w/ check_overflow).
// Absent optional lumps return {present=false}; violations → BspCorruptLump.
[[nodiscard]] std::expected<LumpView, ::xash::core::ErrorCode>
resolve_lump( std::span<const std::byte> file, const HeaderInfo &hi, int lump ) noexcept;

// ---------------------------------------------------------------------------
// Loader pipeline — heap-builder stages (legacy Mod_Load* functions).
// Stages run in the legacy load order and are the only code with write
// access to WorldData (friend).  Each returns BspCorruptLump/BspBadWorld on
// a violation; absent optional lumps succeed with empty output.
// ---------------------------------------------------------------------------

struct LoadContext
{
    std::span<const std::byte>                 file;
    HeaderInfo                                 hi;
    ::xash::map_loader::WorldLoadOptions       opts;
    std::string_view                           name; // diagnostics only
};

// Cross-stage scratch state that does not survive into WorldData.
struct LoadScratch
{
    // Mod_LoadClipnodes output: every source variant widened to 32-bit.
    // Consumed by setup_submodels (shared directly for non-BSP30ext maps;
    // per-hull remap source for BSP30ext), then discarded.
    std::vector<::xash::map_loader::ClipNode32> clipnodes_widened;
};

struct WorldDataFill
{
    using Result = std::expected<void, ::xash::core::ErrorCode>;
    using World  = ::xash::map_loader::WorldData;

    static void begin( const LoadContext &ctx, World &w );     // version/name stamp

    static Result entities    ( const LoadContext &ctx, World &w ); // Mod_LoadEntities (+worldspawn scan)
    static Result planes      ( const LoadContext &ctx, World &w ); // Mod_LoadPlanes (signbits)
    static Result submodels   ( const LoadContext &ctx, World &w ); // Mod_LoadSubmodels (bounds spread)
    static Result visibility  ( const LoadContext &ctx, World &w ); // Mod_LoadVisibility (raw copy)
    static Result marksurfaces( const LoadContext &ctx, World &w ); // Mod_LoadMarkSurfaces (fix-ups)
    static Result leafs       ( const LoadContext &ctx, World &w ); // Mod_LoadLeafs (clusters, leaf-0 check,
                                                                    //   water-alpha probe)
    static Result nodes       ( const LoadContext &ctx, World &w ); // Mod_LoadNodes (no parent links)

    // bsp_flags.cpp — name/flag subset of the texture pipeline
    static Result textures    ( const LoadContext &ctx, World &w ); // Mod_LoadTextures (names only)
    static Result texinfo     ( const LoadContext &ctx, World &w ); // Mod_LoadTexInfo (miptex clamp + flags)
    static Result surfaces    ( const LoadContext &ctx, World &w ); // Mod_LoadSurfaces (SURF_* flags only)

    // map_crc.cpp
    static Result checksum    ( const LoadContext &ctx, World &w ); // CRC32_MapFile

    // bsp_hulls.cpp
    static Result clipnodes       ( const LoadContext &ctx, World &w, LoadScratch &s ); // Mod_LoadClipnodes (widen + aguirRe fix)
    static Result make_hull0      ( const LoadContext &ctx, World &w );                 // Mod_MakeHull0
    static Result setup_submodels ( const LoadContext &ctx, World &w, LoadScratch &s ); // Mod_SetupSubmodels + Mod_SetupHull

    static Result finalize    ( const LoadContext &ctx, World &w ); // required-lump presence checks
};

} // namespace xash::map_loader::bsp
