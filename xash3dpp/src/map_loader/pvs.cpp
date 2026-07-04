// xash3dpp — PVS queries
// Legacy reference: engine/common/mod_bmodel.c :1059-1341 (see header).
//
// Parity notes (Known Deviations in the boundary doc):
//  - decompress_pvs zero-fills when the compressed stream is exhausted
//    (legacy reads past the buffer);
//  - out-of-range leaf indices / visofs values are treated like a missing
//    vis pointer (empty span → full visibility) instead of undefined reads;
//  - the FatPVS recursion is an explicit stack (legacy recurses; behaviour
//    identical for well-formed trees).

#include <xash3dpp/map_loader/pvs.hpp>

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/private/map_loader/fat_vis.hpp>
#include <xash3dpp/private/map_loader/trace_math.hpp>

#include <cstring>
#include <vector>

namespace xash::map_loader {

void decompress_pvs( std::span<const std::byte> in, std::size_t visbytes,
                     std::span<std::byte> out ) noexcept
{
    std::size_t dst = 0;
    const std::size_t limit = visbytes < out.size() ? visbytes : out.size();

    if ( in.empty() )
    {
        // Legacy NULL input: no vis info == all visible.
        std::memset( out.data(), 0xFF, limit );
        return;
    }

    std::size_t src = 0;
    while ( dst < limit )
    {
        if ( src >= in.size() )
        {
            // Hardening: legacy walks past the compressed buffer here.
            std::memset( out.data() + dst, 0, limit - dst );
            return;
        }

        const auto b = static_cast<unsigned char>( in[src] );
        if ( b != 0 )
        {
            out[dst++] = in[src++];
            continue;
        }

        // zero byte + run length of zero bytes (clamped to the output end)
        std::size_t run = src + 1 < in.size()
            ? static_cast<unsigned char>( in[src + 1] )
            : 0;
        if ( run > limit - dst )
            run = limit - dst;
        std::memset( out.data() + dst, 0, run );
        src += 2;
        dst += run;
    }
}

int point_leaf( const WorldData &w, const ::xash::utilities::Vec3 &p ) noexcept
{
    const auto nodes  = w.nodes();
    const auto planes = w.planes();

    int num = 0;
    for ( ;; )
    {
        const Node &node = nodes[static_cast<std::size_t>( num )];
        // On-plane → back child: PlaneDiff(p, plane) <= 0 selects child 1.
        const int side = plane_diff( p, planes[static_cast<std::size_t>( node.planenum )] ) <= 0.0f;
        const int c    = node.children[side];
        if ( c < 0 )
            return -1 - c; // leaf index (disk semantics)
        num = c;
    }
}

std::span<const std::byte> leaf_compressed_pvs( const WorldData &w, int leaf ) noexcept
{
    if ( leaf < 0 || static_cast<std::size_t>( leaf ) >= w.leafs().size() )
        return {};
    const int visofs = w.leafs()[static_cast<std::size_t>( leaf )].visofs;
    if ( visofs < 0 || static_cast<std::size_t>( visofs ) >= w.visdata().size() )
        return {};
    return w.visdata().subspan( static_cast<std::size_t>( visofs ));
}

bool pvs_for_point( const WorldData &w, const ::xash::utilities::Vec3 &p,
                    std::span<std::byte> out ) noexcept
{
    const int leaf = point_leaf( w, p );
    if ( w.leafs()[static_cast<std::size_t>( leaf )].cluster < 0 )
        return false; // legacy returns NULL

    decompress_pvs( leaf_compressed_pvs( w, leaf ), w.visbytes(), out );
    return true;
}

// ---------------------------------------------------------------------------
// box leaf listing
// ---------------------------------------------------------------------------

namespace {

struct LeafList
{
    std::span<int> list;
    std::size_t    count      = 0;
    bool           overflowed = false;
    int            topnode    = -1;
};

void box_leafnums_r( const WorldData &w, const ::xash::utilities::Vec3 &mins,
                     const ::xash::utilities::Vec3 &maxs, LeafList &ll,
                     int node_index ) noexcept
{
    const auto nodes  = w.nodes();
    const auto planes = w.planes();
    const auto leafs  = w.leafs();

    for ( ;; )
    {
        if ( node_index < 0 )
        {
            const Leaf &leaf = leafs[static_cast<std::size_t>( -1 - node_index )];
            if ( leaf.contents == k_contents_solid )
                return; // legacy prunes solid leafs first
            if ( ll.count >= ll.list.size() )
            {
                ll.overflowed = true;
                return;
            }
            ll.list[ll.count++] = leaf.cluster; // clusters, not indices
            return;
        }

        const Node &node = nodes[static_cast<std::size_t>( node_index )];
        const int sides  = box_on_plane_side(
            mins, maxs, planes[static_cast<std::size_t>( node.planenum )] );

        if ( sides == 1 )
            node_index = node.children[0];
        else if ( sides == 2 )
            node_index = node.children[1];
        else
        {
            // go down both
            if ( ll.topnode == -1 )
                ll.topnode = node_index;
            box_leafnums_r( w, mins, maxs, ll, node.children[0] );
            node_index = node.children[1];
        }
    }
}

} // namespace

std::size_t box_leafnums( const WorldData &w, const ::xash::utilities::Vec3 &mins,
                          const ::xash::utilities::Vec3 &maxs,
                          std::span<int> list, int *topnode ) noexcept
{
    LeafList ll;
    ll.list = list;

    box_leafnums_r( w, mins, maxs, ll, 0 );

    if ( topnode )
        *topnode = ll.topnode;
    return ll.count;
}

bool box_visible( const WorldData &w, const ::xash::utilities::Vec3 &mins,
                  const ::xash::utilities::Vec3 &maxs,
                  std::span<const std::byte> visbits ) noexcept
{
    if ( visbits.empty() )
        return true; // legacy NULL visbits

    int clusters[k_max_box_leafs];
    const std::size_t count = box_leafnums( w, mins, maxs, clusters, nullptr );

    for ( std::size_t i = 0; i < count; ++i )
    {
        if ( check_vis_bit( visbits, clusters[i] ))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// fat PVS
// ---------------------------------------------------------------------------

std::size_t fat_pvs( const WorldData &w, const ::xash::utilities::Vec3 &org,
                     float radius, std::span<std::byte> visbuffer,
                     bool merge, bool fullvis ) noexcept
{
    const std::size_t bytes = w.visbytes() < visbuffer.size()
        ? w.visbytes() : visbuffer.size();

    const int leaf = point_leaf( w, org );
    const bool clusterless =
        w.leafs()[static_cast<std::size_t>( leaf )].cluster < 0;

    // "enable full visibility for some reasons" (legacy).
    if ( fullvis || w.visdata().empty() || clusterless )
    {
        std::memset( visbuffer.data(), 0xFF, bytes );
        return bytes;
    }

    if ( !merge )
        std::memset( visbuffer.data(), 0x00, bytes );

    // Shared walk (private/map_loader/fat_vis.hpp); the PVS row source is
    // the leaf's own compressed run.
    detail::fat_vis_walk( w, org, radius, visbuffer, bytes,
                          [&w]( int leaf_index, int /*cluster*/ )
                          {
                              return leaf_compressed_pvs( w, leaf_index );
                          } );

    return bytes;
}

} // namespace xash::map_loader
