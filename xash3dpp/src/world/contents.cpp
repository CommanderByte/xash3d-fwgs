// xash3dpp — point contents + water links + brush-trigger test (S5c)
// Legacy reference: engine/server/sv_world.c :506-567 (trigger accuracy),
// :715-819 (SV_WaterLinks / SV_TruePointContents / SV_PointContents);
// engine/common/world.h :82-101 (RankForContents).
//
// Existing subsystems used:
//   xash3dpp_map_loader — hull point contents, contents constants
//   xash3dpp_utilities  — Matrix3x4 rotation for water/trigger bmodels

#include <xash3dpp/world/trace.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/utilities/matrix.hpp>

namespace xash::server {

namespace ml  = ::xash::map_loader;
namespace abi = ::xash::abi;
namespace ut  = ::xash::utilities;

namespace {

[[nodiscard]] bool vector_is_null( const Vec3 &v ) noexcept
{
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

[[nodiscard]] bool bounds_intersect( const Vec3 &min1, const Vec3 &max1,
                                     const Vec3 &min2, const Vec3 &max2 ) noexcept
{
    if ( min1.x > max2.x || min1.y > max2.y || min1.z > max2.z )
        return false;
    if ( max1.x < min2.x || max1.y < min2.y || max1.z < min2.z )
        return false;
    return true;
}

// The shared rotated-bmodel local-point recipe (sv_world.c:552-563 and
// :754-764): rotate when the model carries an origin brush and the entity
// has angles; otherwise plain offset subtraction.
[[nodiscard]] Vec3 local_test_point( const BrushModel &bm,
                                     const EntityView &bmodel_ent,
                                     const Vec3 &offset,
                                     const Vec3 &point ) noexcept
{
    if ( bm.has_origin && !vector_is_null( bmodel_ent.angles() ))
    {
        const ut::Matrix3x4 m =
            ut::from_angles( offset, bmodel_ent.angles() );
        return ut::transform_point( ut::invert_ortho( m ), point );
    }
    return point - offset;
}

} // namespace

int rank_for_contents( int contents ) noexcept
{
    switch ( contents )
    {
    case ml::k_contents_empty:        return 0;
    case ml::k_contents_water:        return 1;
    case ml::k_contents_translucent:  return 2;
    case ml::k_contents_current_0:    return 3;
    case ml::k_contents_current_90:   return 4;
    case ml::k_contents_current_180:  return 5;
    case ml::k_contents_current_270:  return 6;
    case ml::k_contents_current_up:   return 7;
    case ml::k_contents_current_down: return 8;
    case ml::k_contents_slime:        return 9;
    case ml::k_contents_lava:         return 10;
    case ml::k_contents_sky:          return 11;
    case ml::k_contents_solid:        return 12;
    default:                          return 13; // user contents win
    }
}

namespace {

void water_links( const MoveEnv &env, const Vec3 &origin, int &contents,
                  AreaNode *node ) noexcept
{
    abi::link_t *next = nullptr;
    for ( abi::link_t *l = node->solid_edicts.next;
          l != &node->solid_edicts; l = next )
    {
        next = l->next;
        abi::edict_t    *touch = edict_from_area( l );
        const EntityView tv( touch );

        if ( tv.solid() != abi::k_solid_not ) // disabled ?
            continue;

        if ( tv.groupinfo() != 0 )
        {
            const bool overlap = ( tv.groupinfo() & env.group_mask ) != 0;
            if ( env.group_op == GroupOp::And && !overlap )
                continue;
            if ( env.group_op == GroupOp::Nand && overlap )
                continue;
        }

        // only brushes can have special contents
        const auto bm = env.models->brush_model( tv.modelindex() );
        if ( !bm.has_value() )
            continue;

        if ( !bounds_intersect( origin, origin, tv.absmin(), tv.absmax() ))
            continue;

        // check water brushes accuracy: point probe → hull 0
        const auto sel = hull_for_bsp_entity( env, touch, {}, {} );
        if ( !sel.has_value() )
            continue;

        const Vec3 test = local_test_point( *bm, tv, sel->offset, origin );

        if ( ml::hull_point_contents( sel->hull, sel->hull.firstclipnode,
                                      test ) == ml::k_contents_empty )
            continue;

        // compare contents ranking — new content wins on higher priority
        if ( rank_for_contents( tv.skin() ) > rank_for_contents( contents ))
            contents = tv.skin();
    }

    // recurse down both sides
    if ( node->axis == -1 )
        return;

    if ( vec_axis( origin, node->axis ) > node->dist )
        water_links( env, origin, contents, node->children[0] );
    if ( vec_axis( origin, node->axis ) < node->dist )
        water_links( env, origin, contents, node->children[1] );
}

} // namespace

int true_point_contents( const MoveEnv &env, const Vec3 &p ) noexcept
{
    if ( env.world == nullptr )
        return ml::k_contents_none;

    // get base contents from world (hull 0 of submodel 0, no offset)
    const ml::TraceHull hull0 = ml::world_hull( *env.world, 0, 0 );
    int contents = ml::hull_point_contents( hull0, hull0.firstclipnode, p );

    // check all water entities
    if ( env.area_root != nullptr )
        water_links( env, p, contents, env.area_root );

    return contents;
}

int point_contents( const MoveEnv &env, const Vec3 &p ) noexcept
{
    const int contents = true_point_contents( env, p );

    if ( contents <= ml::k_contents_current_0 &&
         contents >= ml::k_contents_current_down )
        return ml::k_contents_water;
    return contents;
}

bool brush_trigger_intersects( const MoveEnv &env,
                               abi::edict_t *trigger,
                               abi::edict_t *ent ) noexcept
{
    const EntityView tv( trigger );
    const EntityView ev( ent );

    // Non-brush triggers keep the AABB verdict (legacy only refines
    // mod_brush models).
    const auto bm = env.models->brush_model( tv.modelindex() );
    if ( !bm.has_value() )
        return true;

    // force to select bsp-hull at the TOUCHER's size
    const auto sel = hull_for_bsp_entity( env, trigger, ev.mins(),
                                          ev.maxs() );
    if ( !sel.has_value() )
        return true;

    const Vec3 test = local_test_point( *bm, tv, sel->offset, ev.origin() );

    return ml::hull_point_contents( sel->hull, sel->hull.firstclipnode,
                                    test ) == ml::k_contents_solid;
}

} // namespace xash::server
