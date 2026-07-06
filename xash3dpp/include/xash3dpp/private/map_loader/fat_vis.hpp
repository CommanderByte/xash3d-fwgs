#pragma once
// xash3dpp — shared fat-vis BSP walk (Mod_FatPVS_RecursiveBSPNode's node
// descent, mod_bmodel.c:1168).  The row source is injected: the PVS path
// reads the leaf's own compressed vis run, the PHS path (Q-19) reads the
// PhsTable row for cluster + 1.  Both decompress at `bytes` width and OR
// into the caller's buffer.
//
// The legacy recursion is replaced by an explicit stack (behaviour
// identical for well-formed trees — pvs.cpp Known-Deviations note).

#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/map_loader/trace_math.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace xash::map_loader::detail {

// RowSource: std::span<const std::byte>( int leaf_index, int cluster ) —
// the compressed zero-RLE run for that leaf's row (to-end span; empty ==
// "no vis" == all-visible per decompress_pvs).  Called only for leafs
// with cluster >= 0.
template <class RowSource>
void fat_vis_walk( const WorldData &w, const ::xash::utilities::Vec3 &org,
                   float radius, std::span<std::byte> visbuffer,
                   std::size_t bytes, RowSource row_source ) noexcept
{
    const auto nodes  = w.nodes();
    const auto planes = w.planes();
    const auto leafs  = w.leafs();

    // Per-leaf decompression scratch (legacy uses the static g_visdata row).
    std::vector<std::byte> row( bytes );

    std::vector<int> stack; // @pre-reserved: query-local BSP-descent stack, bounded by node-tree depth, freed per call (reserve deferred — Q-18-gated query path, sizing is a parity-safe follow-up)
    stack.push_back( 0 );

    while ( !stack.empty() )
    {
        int num = stack.back();
        stack.pop_back();

        while ( num >= 0 )
        {
            const Node &node = nodes[static_cast<std::size_t>( num )];
            const float d = plane_diff( org, planes[static_cast<std::size_t>( node.planenum )] );

            if ( d > radius )
                num = node.children[0];
            else if ( d < -radius )
                num = node.children[1];
            else
            {
                // go down both sides
                stack.push_back( node.children[0] );
                num = node.children[1];
            }
        }

        const int   leaf_index = -1 - num;
        const Leaf &leaf_hit   = leafs[static_cast<std::size_t>( leaf_index )];
        if ( leaf_hit.cluster < 0 )
            continue;

        decompress_pvs( row_source( leaf_index, leaf_hit.cluster ), bytes, row );
        for ( std::size_t i = 0; i < bytes; ++i )
            visbuffer[i] = static_cast<std::byte>(
                static_cast<unsigned char>( visbuffer[i] ) |
                static_cast<unsigned char>( row[i] ));
    }
}

} // namespace xash::map_loader::detail
