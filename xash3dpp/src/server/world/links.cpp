// xash3dpp — areanode linking implementation (Chunk 6 S5a)
// Legacy reference: engine/server/sv_world.c :411-706
//
// Existing subsystems used:
//   xash3dpp_map_loader — box_leafnums (cluster fill + straddle topnode)
//   xash3dpp_core       — thread-role assertion (OQ-9: main-thread only)

#include <xash3dpp/private/server/world_links.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/private/server/entity_view.hpp>

#include <cstring>

namespace xash::server {

namespace ml  = ::xash::map_loader;
namespace abi = ::xash::abi;

namespace {

void set_axis( Vec3 &v, int axis, float value ) noexcept
{
    if ( axis == 0 )
        v.x = value;
    else if ( axis == 1 )
        v.y = value;
    else
        v.z = value;
}

// Legacy BoundsIntersect (world.h): strict-inequality reject per axis.
[[nodiscard]] bool bounds_intersect( const Vec3 &min1, const Vec3 &max1,
                                     const Vec3 &min2, const Vec3 &max2 ) noexcept
{
    if ( min1.x > max2.x || min1.y > max2.y || min1.z > max2.z )
        return false;
    if ( max1.x < min2.x || max1.y < min2.y || max1.z < min2.z )
        return false;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// tree construction
// ---------------------------------------------------------------------------

AreaNode *WorldLinks::create_node( int depth, const Vec3 &mins,
                                   const Vec3 &maxs )
{
    AreaNode *anode = &nodes_[num_nodes_++];

    clear_link( anode->trigger_edicts );
    clear_link( anode->solid_edicts );
    clear_link( anode->portal_edicts );

    if ( depth == static_cast<int>( ::xash::limits::server_area_depth ))
    {
        anode->axis        = -1;
        anode->children[0] = anode->children[1] = nullptr;
        return anode;
    }

    // Split the longer of X/Y — never Z (legacy).
    const Vec3 size = { maxs.x - mins.x, maxs.y - mins.y, maxs.z - mins.z };
    anode->axis = size.x > size.y ? 0 : 1;
    anode->dist = 0.5f * ( vec_axis( maxs, anode->axis ) +
                           vec_axis( mins, anode->axis ));

    Vec3 mins1 = mins, mins2 = mins;
    Vec3 maxs1 = maxs, maxs2 = maxs;
    set_axis( maxs1, anode->axis, anode->dist );
    set_axis( mins2, anode->axis, anode->dist );

    anode->children[0] = create_node( depth + 1, mins2, maxs2 );
    anode->children[1] = create_node( depth + 1, mins1, maxs1 );

    return anode;
}

void WorldLinks::clear_world( const Vec3 &world_mins, const Vec3 &world_maxs )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for ( AreaNode &n : nodes_ )
        n = AreaNode{};
    num_nodes_       = 0;
    touch_semaphore_ = false;

    create_node( 0, world_mins, world_maxs );
}

// ---------------------------------------------------------------------------
// unlink / leaf fill
// ---------------------------------------------------------------------------

void WorldLinks::unlink_edict( abi::edict_t *ent ) noexcept
{
    if ( !ent->area.prev )
        return; // not linked in anywhere

    remove_link( ent->area );
    ent->area.prev = nullptr;
    ent->area.next = nullptr;
}

void WorldLinks::find_touched_leafs( abi::edict_t *ent, const LinkEnv &env )
{
    const EntityView view( ent );

    ent->num_leafs = 0;
    ent->headnode  = -1;

    if ( view.modelindex() == 0 || env.world == nullptr )
        return;

    const bool qbsp2 =
        env.world->version() == ml::BspVersion::Bsp2;
    const std::size_t max_leafs = qbsp2
        ? static_cast<std::size_t>( abi::k_max_ent_leafs_32 )
        : static_cast<std::size_t>( abi::k_max_ent_leafs_16 );

    // One extra slot disambiguates "exactly full" from "overflowed"
    // (legacy counts one past MAX as its overflow sentinel).
    int clusters[abi::k_max_ent_leafs_16 + 1];
    int topnode = -1;

    const std::size_t count = ml::box_leafnums(
        *env.world, view.absmin(), view.absmax(),
        std::span<int>( clusters, max_leafs + 1 ), &topnode );

    if ( count > max_leafs )
    {
        // Too many leafs for individual storage — use the headnode
        // (first straddling node) for visibility checks instead.
        std::memset( ent->leafnums32, -1, sizeof( ent->leafnums32 ));
        ent->num_leafs = 0;
        ent->headnode  = topnode;
        return;
    }

    for ( std::size_t i = 0; i < count; ++i )
    {
        if ( qbsp2 )
            ent->leafnums32[i] = clusters[i];
        else
            ent->leafnums16[i] = static_cast<short>( clusters[i] );
    }
    ent->num_leafs = static_cast<int>( count );
}

// ---------------------------------------------------------------------------
// trigger touching
// ---------------------------------------------------------------------------

void WorldLinks::touch_links( abi::edict_t *ent, AreaNode *node,
                              const LinkEnv &env )
{
    const EntityView view( ent );

    // touch linked edicts (save next — the touch callback may relink)
    abi::link_t *next = nullptr;
    for ( abi::link_t *l = node->trigger_edicts.next;
          l != &node->trigger_edicts; l = next )
    {
        next = l->next;
        abi::edict_t    *touch = edict_from_area( l );
        const EntityView tv( touch );

        // (physFuncs.SV_TriggerTouch override is an S8 physics-interface
        // seam; the built-in filter chain applies until then.)
        if ( touch == ent || tv.solid() != abi::k_solid_trigger )
            continue;

        if ( tv.groupinfo() != 0 && view.groupinfo() != 0 )
        {
            const bool overlap =
                ( tv.groupinfo() & view.groupinfo() ) != 0;
            if ( group_op_ == GroupOp::And && !overlap )
                continue;
            if ( group_op_ == GroupOp::Nand && overlap )
                continue;
        }

        if ( !bounds_intersect( view.absmin(), view.absmax(),
                                tv.absmin(), tv.absmax() ))
            continue;

        // Exact brush-trigger refinement (BSP hull + rotation support) —
        // installed by the hull slice; default accepts the AABB hit.
        if ( hooks_ != nullptr &&
             !hooks_->brush_trigger_intersects( touch, ent ))
            continue;

        // never touch the triggers when "playersonly" is active
        if ( !env.playersonly && hooks_ != nullptr )
            hooks_->dispatch_touch( touch, ent );
    }

    // recurse down both sides
    if ( node->axis == -1 )
        return;

    if ( vec_axis( view.absmax(), node->axis ) > node->dist )
        touch_links( ent, node->children[0], env );
    if ( vec_axis( view.absmin(), node->axis ) < node->dist )
        touch_links( ent, node->children[1], env );
}

// ---------------------------------------------------------------------------
// SV_LinkEdict
// ---------------------------------------------------------------------------

void WorldLinks::link_edict( abi::edict_t *ent, bool touch_triggers,
                             const LinkEnv &env )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( ent->area.prev )
        unlink_edict( ent );          // unlink from old position
    if ( ent == env.worldspawn )
        return;                       // don't add the world
    const EntityView view( ent );
    if ( !view.valid() )
        return;                       // never add freed ents

    // set the abs box — game-DLL responsibility, no engine fallback
    if ( hooks_ != nullptr )
        hooks_->set_abs_box( ent );

    const EntityView aiment( view.aiment() );
    if ( view.movetype() == abi::k_movetype_follow && aiment.valid() )
    {
        std::memcpy( ent->leafnums32, aiment.raw()->leafnums32,
                     sizeof( ent->leafnums32 ));
        ent->num_leafs = aiment.raw()->num_leafs;
        ent->headnode  = aiment.raw()->headnode;
    }
    else
    {
        find_touched_leafs( ent, env );
    }

    // ignore non-solid bodies (water/ladder bmodels have skin < 0 so
    // they DO link into the solid list)
    if ( view.solid() == abi::k_solid_not &&
         view.skin() >= ml::k_contents_empty )
        return;

    // find the first node that the ent's box crosses
    AreaNode *node = &nodes_[0];
    for ( ;; )
    {
        if ( node->axis == -1 )
            break;
        if ( vec_axis( view.absmin(), node->axis ) > node->dist )
            node = node->children[0];
        else if ( vec_axis( view.absmax(), node->axis ) < node->dist )
            node = node->children[1];
        else
            break; // crosses the node
    }

    // link it in
    if ( view.solid() == abi::k_solid_trigger )
        insert_link_before( ent->area, node->trigger_edicts );
    else if ( view.solid() == abi::k_solid_portal )
        insert_link_before( ent->area, node->portal_edicts );
    else
        insert_link_before( ent->area, node->solid_edicts );

    if ( touch_triggers && !touch_semaphore_ )
    {
        touch_semaphore_ = true;
        touch_links( ent, &nodes_[0], env );
        touch_semaphore_ = false;
    }
}

} // namespace xash::server
