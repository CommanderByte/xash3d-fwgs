#pragma once
// xash3dpp — immutable world model loaded from a BSP file.
// Legacy reference: engine/common/mod_bmodel.c, common/com_model.h
// Deep dives: docs/legacy-survey/deep-dive-bsp-loader.md,
//             docs/legacy-survey/deep-dive-trace-pvs.md
//
// WorldData is the normalized in-memory form: all on-disk variance
// (v29/v30/BSP2/BSP30ext record widths, Blue-Shift lump swap, broken-compiler
// fix-ups) is resolved at load time.  After load_world_data() returns, the
// object is IMMUTABLE — every accessor is const and all spatial queries take
// const WorldData& — which is what makes world queries concurrent-read-safe
// after map activation (Q-6).
//
// Chunk 5 build-up: C4 = core lumps (entities/planes/submodels/visibility/
// marksurfaces/leafs/nodes); C5 adds clipnodes + hulls; C6 adds surface
// content flags, water-alpha and the map checksum.

#include <xash3dpp/core/error.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem { class Filesystem; }

namespace xash::map_loader {

namespace bsp { struct WorldDataFill; }

// Detected BSP flavour.  Informational (diagnostics, mapstats) — queries
// never branch on it; record widths are normalized during load.
enum class BspVersion : std::uint8_t
{
    Quake1,      // version 29
    HalfLife,    // version 30
    HalfLifeExt, // version 30 + 'XASH' extra header (BSP30ext large-map support)
    Bsp2,        // 'BSP2' fourcc (32-bit records everywhere)
};

// ---------------------------------------------------------------------------
// In-memory record types (normalized)
// ---------------------------------------------------------------------------

// Legacy mplane_t.  signbits is computed at load exactly as Mod_LoadPlanes
// does (bit j set when normal[j] < 0).
struct Plane
{
    ::xash::utilities::Vec3 normal;
    float                   dist;
    std::uint8_t            type;     // PLANE_X/Y/Z (<3: axial fast path) or non-axial
    std::uint8_t            signbits; // signx | signy<<1 | signz<<2
};

// Always-32-bit clipnode — the single normalized form (legacy widens all
// variants to this during load and only narrows back for the frozen model_t
// layout, which xash3dpp does not expose).  children[i] >= 0 is a clipnode
// index; < 0 is a CONTENTS_* value.
struct ClipNode32
{
    int planenum;
    int children[2];
};

// BSP node.  children[i] >= 0 is a node index; < 0 refers to leaf (-1 - child)
// — disk semantics kept so query code reads like the legacy walkers.
struct Node
{
    int                     planenum;
    int                     children[2];
    ::xash::utilities::Vec3 mins, maxs;
    int                     firstsurface;
    int                     numsurfaces;
};

// BSP leaf.  cluster is derived at load (leaf i → i-1, clamped to -1 when
// >= visclusters; always -1 for non-world models and the solid leaf 0).
// visofs is kept RAW, unclamped — legacy parity (mod_bmodel.c:3701-3702
// assigns visdata+visofs with no bounds check; PVS code gates on cluster).
struct Leaf
{
    int                         contents; // k_contents_* value
    int                         cluster;
    int                         visofs;   // byte offset into visdata(); -1 = none
    ::xash::utilities::Vec3     mins, maxs;
    int                         firstmarksurface;
    int                         nummarksurfaces;
    std::array<std::uint8_t, 4> ambient_sound_level;
};

// Per-hull clipnode span + Minkowski padding, wired by Mod_SetupHull /
// Mod_MakeHull0 equivalents.  `present == false` is the legacy
// "hull->planes == NULL" marker: point-contents answers CONTENTS_NONE and
// traces treat the hull as open.  Hull index 0 walks hull0_nodes();
// hulls 1-3 walk clipnodes().
struct HullDescriptor
{
    int                     firstclipnode = 0;
    int                     lastclipnode  = 0;
    ::xash::utilities::Vec3 clip_mins{}, clip_maxs{};
    bool                    present = false;
};

// SubModel::flags bits — ABI values from engine/ref_api.h:97-100 (the
// legacy model_t.flags the server/renderer read).
inline constexpr std::uint32_t k_model_conveyor    = 1u << 0;
inline constexpr std::uint32_t k_model_has_origin  = 1u << 1;
inline constexpr std::uint32_t k_model_liquid      = 1u << 2;
inline constexpr std::uint32_t k_model_transparent = 1u << 3;

// Legacy dmodel_t / "*N" inline brush model.  Bounds are spread by one unit
// at load (legacy Mod_LoadSubmodels).  k_model_* flag bits: origin detection
// in C5, surface-derived conveyor/transparent/liquid in C6.
struct SubModel
{
    ::xash::utilities::Vec3       mins{}, maxs{}, origin{};
    std::array<int, 4>            headnode{};
    std::array<HullDescriptor, 4> hulls{};
    int                           visleafs   = 0;
    int                           firstface  = 0;
    int                           numfaces   = 0;
    std::uint32_t                 flags      = 0;
};

// ---------------------------------------------------------------------------
// Hull dimension table
// ---------------------------------------------------------------------------

// Player hull extents indexed by usehull (0 standing / 1 ducked / 2 point /
// 3 large).  Defaults are the engine tables from pm_trace.c:30-45; the game
// DLL may override via pfnGetHullBounds (Chunk 6), which is why the table is
// injectable through WorldLoadOptions.
struct HullBounds
{
    ::xash::utilities::Vec3 mins, maxs;
};
using HullBoundsTable = std::array<HullBounds, 4>;

inline constexpr HullBoundsTable k_default_hull_bounds = { {
    { { -16.0f, -16.0f, -36.0f }, { 16.0f, 16.0f, 36.0f } }, // 0: standing
    { { -16.0f, -16.0f, -18.0f }, { 16.0f, 16.0f, 18.0f } }, // 1: ducked
    { {   0.0f,   0.0f,   0.0f }, {  0.0f,  0.0f,  0.0f } }, // 2: point
    { { -32.0f, -32.0f, -32.0f }, { 32.0f, 32.0f, 32.0f } }, // 3: large
} };

struct WorldLoadOptions
{
    bool            is_world = true;         // world semantics: visdata, clusters,
                                             // worldspawn scan, leaf-0-solid check
    bool            multiplayer_crc = false; // checksum(): real CRC vs SP constant (C6)
    HullBoundsTable hull_bounds = k_default_hull_bounds;
};

// ---------------------------------------------------------------------------
// WorldData
// ---------------------------------------------------------------------------

class WorldData
{
public:
    WorldData() noexcept;
    ~WorldData();

    WorldData( const WorldData & )            = delete;
    WorldData &operator=( const WorldData & ) = delete;

    WorldData( WorldData && ) noexcept;
    WorldData &operator=( WorldData && ) noexcept;

    // All accessors are const and concurrent-read-safe after load (Q-6).
    [[nodiscard]] BspVersion                    version()      const noexcept;
    [[nodiscard]] std::uint32_t                 flags()        const noexcept; // FWORLD_*-equivalent bits (C6)
    [[nodiscard]] std::uint32_t                 checksum()     const noexcept; // map CRC (C6)
    [[nodiscard]] std::string_view              name()         const noexcept;

    [[nodiscard]] std::span<const Plane>        planes()       const noexcept;
    [[nodiscard]] std::span<const Node>         nodes()        const noexcept;
    [[nodiscard]] std::span<const Leaf>         leafs()        const noexcept;
    [[nodiscard]] std::span<const int>          marksurfaces() const noexcept;
    [[nodiscard]] std::span<const SubModel>     submodels()    const noexcept; // [0] = world
    [[nodiscard]] std::span<const ClipNode32>   clipnodes()    const noexcept; // shared hull 1-3 array (C5)
    [[nodiscard]] std::span<const ClipNode32>   hull0_nodes()  const noexcept; // MakeHull0 output (C5)

    [[nodiscard]] std::span<const std::byte>    visdata()      const noexcept; // raw compressed PVS
    [[nodiscard]] int                           visclusters()  const noexcept;
    [[nodiscard]] std::size_t                   visbytes()     const noexcept; // (visclusters+7)>>3

    [[nodiscard]] std::string_view              entities()     const noexcept; // raw lump text (NUL-safe)
    [[nodiscard]] std::string_view              wadlist()      const noexcept; // worldspawn "wad" value, raw
    [[nodiscard]] std::string_view              message()      const noexcept; // worldspawn "message" value

private:
    friend struct bsp::WorldDataFill;

    BspVersion    version_     = BspVersion::HalfLife;
    std::uint32_t flags_       = 0;
    std::uint32_t checksum_    = 0;
    int           visclusters_ = 0;
    std::size_t   visbytes_    = 0;

    std::string name_;
    std::string entities_;
    std::string wadlist_;
    std::string message_;

    std::vector<Plane>      planes_;
    std::vector<Node>       nodes_;
    std::vector<Leaf>       leafs_;
    std::vector<int>        marksurfaces_;
    std::vector<SubModel>   submodels_;
    std::vector<ClipNode32> clipnodes_;
    std::vector<ClipNode32> hull0_nodes_;
    std::vector<std::byte>  visdata_;
};

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

// Parses a BSP file image into an immutable WorldData.  `name` is used for
// diagnostics only.  Errors: BspCorruptLump / BspUnsupportedVersion /
// BspBadWorld (all logged at tag "map_loader", Q-5).
[[nodiscard]] std::expected<WorldData, ::xash::core::ErrorCode>
load_world_data( std::span<const std::byte> file, std::string_view name,
                 const WorldLoadOptions &opts ) noexcept;

// Convenience: loads `path` through the virtual filesystem, then delegates.
// Additional error: MapNotFound when the file cannot be read.
[[nodiscard]] std::expected<WorldData, ::xash::core::ErrorCode>
load_world_data( ::xash::filesystem::Filesystem &fs, std::string_view path,
                 const WorldLoadOptions &opts ) noexcept;

} // namespace xash::map_loader
