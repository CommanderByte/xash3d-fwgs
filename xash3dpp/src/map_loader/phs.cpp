// xash3dpp — PHS build + queries (Chunk 6, Q-19)
// Legacy reference: engine/common/mod_bmodel.c :1088 (Mod_CompressPVS),
// :3730 (Mod_CalcPHS), :1168-1241 (Mod_FatPVS phs path);
// engine/server/sv_game.c :4299 (Mod_HeadnodeVisible).
//
// Parity notes (Known Deviations in the map_loader boundary doc):
//  - the OpenMP build parallelism is not ported (single-threaded fold;
//    output is byte-identical — the loop is embarrassingly parallel);
//  - compressed_row() bounds-checks the row index (legacy indexes phsofs
//    unchecked); an out-of-range row decompresses as all-visible, the
//    module's standing hardening convention;
//  - the vis_stats developer counters (host_developer >= 2) are not
//    ported (pure logging, no data effect).

#include <xash3dpp/map_loader/phs.hpp>

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/private/map_loader/fat_vis.hpp>

#include <cstdint>
#include <cstring>

namespace xash::map_loader {

namespace {

// Legacy forces 32-bit row alignment: rowbytes = ALIGN(visbytes, 4).
inline constexpr std::size_t k_phs_row_align = 4;
// Zero-RLE run length is stored in one byte.
inline constexpr std::size_t k_rle_max_run = 255;

[[nodiscard]] std::size_t align_row( std::size_t v ) noexcept
{
    return ( v + ( k_phs_row_align - 1 )) & ~( k_phs_row_align - 1 );
}

} // namespace

// ---------------------------------------------------------------------------
// PhsTable
// ---------------------------------------------------------------------------

std::span<const std::byte> PhsTable::compressed_row( std::size_t i ) const noexcept
{
    if ( i >= offsets_.size() )
        return {};
    // To-end span — decompression consumes what it needs (legacy pointer
    // semantics; rows are self-terminating at the visbytes limit).
    return std::span<const std::byte>( blob_ ).subspan( offsets_[i] );
}

// ---------------------------------------------------------------------------
// codec
// ---------------------------------------------------------------------------

std::size_t compress_pvs( std::span<const std::byte> in,
                          std::span<std::byte> out ) noexcept
{
    std::size_t dst = 0;

    for ( std::size_t i = 0; i < in.size(); ++i )
    {
        out[dst++] = in[i];

        // only compress zeros
        if ( static_cast<unsigned char>( in[i] ) != 0 )
            continue;

        std::size_t j = i + 1, rep = 1;
        for ( ; j < in.size() && rep != k_rle_max_run; ++j, ++rep )
        {
            if ( static_cast<unsigned char>( in[j] ) != 0 )
                break;
        }
        out[dst++] = static_cast<std::byte>( rep );
        i = j - 1;
    }

    return dst;
}

// ---------------------------------------------------------------------------
// build (Mod_CalcPHS)
// ---------------------------------------------------------------------------

PhsTable build_phs( const WorldData &w )
{
    PhsTable table;

    if ( w.visdata().empty() )
        return table; // legacy: no visdata → no PHS

    const std::size_t rowbytes = align_row( w.visbytes() );
    const std::size_t count    = w.leafs().size(); // numleafs + 1 (row 0 = solid leaf)

    // Decompress every leaf's PVS row (rowbytes wide, zero padding).
    std::vector<std::byte> pvs_rows( rowbytes * count );
    for ( std::size_t i = 0; i < count; ++i )
    {
        decompress_pvs( leaf_compressed_pvs( w, static_cast<int>( i )),
                        w.visbytes(),
                        std::span<std::byte>( pvs_rows ).subspan( rowbytes * i, rowbytes ));
    }

    // Fold: PHS row i = PVS row i OR PVS row (bit + 1) for every set bit.
    std::vector<std::byte> phs_rows( rowbytes * count );
    for ( std::size_t i = 0; i < count; ++i )
    {
        const std::byte *scan = &pvs_rows[rowbytes * i];
        std::byte       *dst  = &phs_rows[rowbytes * i];

        std::memcpy( dst, scan, rowbytes );

        for ( std::size_t j = 0; j < rowbytes; ++j )
        {
            const std::uint32_t bitbyte = static_cast<unsigned char>( scan[j] );
            if ( bitbyte == 0 )
                continue;

            for ( std::size_t k = 0; k < 8; ++k )
            {
                if ( !( bitbyte & ( 1u << k )))
                    continue;

                // OR this pvs row into the phs (+1: PVS bits are cluster
                // numbers; row index = cluster + 1).
                const std::size_t index = j * 8 + k + 1;
                if ( index >= count )
                    continue;

                const std::byte *src = &pvs_rows[rowbytes * index];
                for ( std::size_t b = 0; b < rowbytes; ++b )
                    dst[b] = static_cast<std::byte>(
                        static_cast<unsigned char>( dst[b] ) |
                        static_cast<unsigned char>( src[b] ));
            }
        }
    }

    // Compress row by row into the blob + offset table (legacy phsofs).
    std::vector<std::byte> scratch( rowbytes * 2 ); // 2x = zero-RLE worst case
    table.offsets_.resize( count );
    for ( std::size_t i = 0; i < count; ++i )
    {
        const std::size_t size = compress_pvs(
            std::span<const std::byte>( phs_rows ).subspan( rowbytes * i, rowbytes ),
            scratch );
        table.offsets_[i] = table.blob_.size();
        table.blob_.insert( table.blob_.end(), scratch.begin(),
                            scratch.begin() + static_cast<std::ptrdiff_t>( size ));
    }

    return table;
}

// ---------------------------------------------------------------------------
// queries
// ---------------------------------------------------------------------------

std::size_t fat_phs( const WorldData &w, const PhsTable &phs,
                     const ::xash::utilities::Vec3 &org, float radius,
                     std::span<std::byte> visbuffer, bool merge,
                     bool fullvis ) noexcept
{
    const std::size_t bytes = w.visbytes() < visbuffer.size()
        ? w.visbytes() : visbuffer.size();

    const int leaf = point_leaf( w, org );
    const bool clusterless =
        w.leafs()[static_cast<std::size_t>( leaf )].cluster < 0;

    if ( fullvis || w.visdata().empty() || clusterless )
    {
        std::memset( visbuffer.data(), 0xFF, bytes );
        return bytes;
    }

    // "requested PHS but we don't have PHS for some reason" (legacy).
    if ( phs.empty() )
    {
        std::memset( visbuffer.data(), 0xFF, bytes );
        return bytes;
    }

    if ( !merge )
        std::memset( visbuffer.data(), 0x00, bytes );

    detail::fat_vis_walk( w, org, radius, visbuffer, bytes,
                          [&phs]( int /*leaf_index*/, int cluster )
                          {
                              return phs.compressed_row(
                                  static_cast<std::size_t>( cluster ) + 1 );
                          } );

    return bytes;
}

bool headnode_visible( const WorldData &w, int headnode,
                       std::span<const std::byte> visbits,
                       int *lastleaf ) noexcept
{
    const auto nodes = w.nodes();
    const auto leafs = w.leafs();

    if ( headnode < 0 || static_cast<std::size_t>( headnode ) >= nodes.size() )
        return false;

    // Explicit stack; child order preserved (front pushed last → visited
    // first) so the first visible leaf matches legacy traversal order.
    std::vector<int> stack;
    stack.push_back( headnode );

    while ( !stack.empty() )
    {
        const int num = stack.back();
        stack.pop_back();

        if ( num < 0 )
        {
            const Leaf &leaf = leafs[static_cast<std::size_t>( -1 - num )];
            if ( leaf.contents == k_contents_solid )
                continue;
            if ( !check_vis_bit( visbits, leaf.cluster ))
                continue;
            if ( lastleaf )
                *lastleaf = leaf.cluster;
            return true;
        }

        const Node &node = nodes[static_cast<std::size_t>( num )];
        stack.push_back( node.children[1] );
        stack.push_back( node.children[0] );
    }

    return false;
}

} // namespace xash::map_loader
