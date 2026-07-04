// xash3dpp — WorldData accessors + load_world_data orchestration
// Legacy reference: engine/common/mod_bmodel.c — Mod_LoadBmodelLumps
// (:4242-4384).  Stage order preserved from the legacy heap-builder sequence
// (entities, planes, submodels, visibility, marksurfaces, leafs, nodes);
// skipped legacy stages (vertexes/edges/surfedges/lighting) are render-only.
//
// Existing subsystems used:
//   xash3dpp_filesystem — whole-file load for the path overload
//   xash3dpp_core       — logging (tag "map_loader", Q-5)

#include <xash3dpp/map_loader/world.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/private/map_loader/bsp/bsp_loader.hpp>

#include <string>
#include <utility>

namespace xash::map_loader {

// ---------------------------------------------------------------------------
// WorldData special members (QJ: copy deleted in header, move defaulted here)
// ---------------------------------------------------------------------------

WorldData::WorldData() noexcept = default;
WorldData::~WorldData()         = default;

WorldData::WorldData( WorldData && ) noexcept            = default;
WorldData &WorldData::operator=( WorldData && ) noexcept = default;

// ---------------------------------------------------------------------------
// accessors
// ---------------------------------------------------------------------------

BspVersion    WorldData::version()  const noexcept { return version_; }
std::uint32_t WorldData::flags()    const noexcept { return flags_; }
std::uint32_t WorldData::checksum() const noexcept { return checksum_; }

std::string_view WorldData::name() const noexcept { return name_; }

std::span<const Plane>      WorldData::planes()       const noexcept { return planes_; }
std::span<const Node>       WorldData::nodes()        const noexcept { return nodes_; }
std::span<const Leaf>       WorldData::leafs()        const noexcept { return leafs_; }
std::span<const int>        WorldData::marksurfaces() const noexcept { return marksurfaces_; }
std::span<const SubModel>   WorldData::submodels()    const noexcept { return submodels_; }
std::span<const ClipNode32> WorldData::clipnodes()    const noexcept { return clipnodes_; }
std::span<const ClipNode32> WorldData::hull0_nodes()  const noexcept { return hull0_nodes_; }

std::span<const std::byte> WorldData::visdata()     const noexcept { return visdata_; }
int                        WorldData::visclusters() const noexcept { return visclusters_; }
std::size_t                WorldData::visbytes()    const noexcept { return visbytes_; }

std::string_view WorldData::entities() const noexcept { return entities_; }
std::string_view WorldData::wadlist()  const noexcept { return wadlist_; }
std::string_view WorldData::message()  const noexcept { return message_; }

// ---------------------------------------------------------------------------
// load_world_data
// ---------------------------------------------------------------------------

std::expected<WorldData, ::xash::core::ErrorCode>
load_world_data( std::span<const std::byte> file, std::string_view name,
                 const WorldLoadOptions &opts ) noexcept
{
    using Fill = bsp::WorldDataFill;

    const auto hi = bsp::parse_header( file );
    if ( !hi )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "load_world_data: '%.*s' failed header parse",
                            static_cast<int>( name.size() ), name.data() );
        return std::unexpected( hi.error() );
    }

    const bsp::LoadContext ctx{ file, *hi, opts, name };

    WorldData w;
    Fill::begin( ctx, w );

    // Legacy heap-builder order (subset; see file header).
    Fill::Result r;
    if ( !( r = Fill::entities( ctx, w )) ||
         !( r = Fill::planes( ctx, w )) ||
         !( r = Fill::submodels( ctx, w )) ||
         !( r = Fill::visibility( ctx, w )) ||
         !( r = Fill::marksurfaces( ctx, w )) ||
         !( r = Fill::leafs( ctx, w )) ||
         !( r = Fill::nodes( ctx, w )) ||
         !( r = Fill::finalize( ctx, w )))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "load_world_data: '%.*s' failed (%s)",
                            static_cast<int>( name.size() ), name.data(),
                            ::xash::core::error_code_name( r.error() ));
        return std::unexpected( r.error() );
    }

    return w;
}

std::expected<WorldData, ::xash::core::ErrorCode>
load_world_data( ::xash::filesystem::Filesystem &fs, std::string_view path,
                 const WorldLoadOptions &opts ) noexcept
{
    const std::vector<std::byte> file = fs.load_file( path );
    if ( file.empty() )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "load_world_data: could not read '%.*s'",
                            static_cast<int>( path.size() ), path.data() );
        return std::unexpected( ::xash::core::ErrorCode::MapNotFound );
    }
    return load_world_data( file, path, opts );
}

} // namespace xash::map_loader
