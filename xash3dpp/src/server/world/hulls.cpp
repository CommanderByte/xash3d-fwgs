// xash3dpp — server hull selection (Chunk 6 S5b)
// Legacy reference: engine/server/sv_world.c :152-273
//
// Existing subsystems used:
//   xash3dpp_map_loader — world_hull views + BoxHull
//   xash3dpp_core       — logging for the legacy Host_Error conditions

#include <xash3dpp/private/server/world_trace.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/private/server/entity_view.hpp>

namespace xash::server {

namespace ml  = ::xash::map_loader;
namespace abi = ::xash::abi;

std::optional<SvHull>
hull_for_bsp_entity( const MoveEnv &env, abi::edict_t *ent,
                     const Vec3 &mins, const Vec3 &maxs ) noexcept
{
    // (physFuncs.SV_HullForBsp override is an S8 physics-interface seam.)
    const EntityView view( ent );

    const auto bm = env.models->brush_model( view.modelindex() );
    if ( !bm.has_value() || env.world == nullptr )
    {
        // Legacy: Host_Error "SOLID_BSP with a non bsp model" — surfaced
        // as an error return per Q-5; the bridge maps it to the host
        // error policy (Known Deviation, server-boundary.md).
        ::xash::core::log( ::xash::core::LogLevel::Error, "server",
                           "SOLID_BSP entity with a non-bsp model" );
        return std::nullopt;
    }

    const Vec3 size = maxs - mins;

    int  hull_index;
    bool point_hull = false;

    if ( env.quake_hull_select )
    {
        // alternate hull select for quake maps (FWORLD_SKYSPHERE)
        if ( size.x < 3.0f || view.solid() == abi::k_solid_portal )
            hull_index = 0;
        else if ( size.x <= 32.0f )
            hull_index = 1;
        else
            hull_index = 2;
    }
    else
    {
        if ( size.x <= 8.0f || view.solid() == abi::k_solid_portal )
        {
            hull_index = 0;
            point_hull = true; // offset = clip_mins VERBATIM (quirk)
        }
        else if ( size.x <= 36.0f )
        {
            hull_index = size.z <= 36.0f ? 3 : 1;
        }
        else
        {
            hull_index = 2;
        }
    }

    SvHull out;
    out.hull = ml::world_hull( *env.world, bm->submodel, hull_index );

    // HL point-hull quirk: hull 0's offset is clip_mins verbatim, NOT
    // clip_mins - mins (sv_world.c:217 vs :229).
    out.offset = point_hull ? out.hull.clip_mins
                            : out.hull.clip_mins - mins;
    out.offset += view.origin();

    return out;
}

std::optional<SvHull>
hull_for_entity( const MoveEnv &env, abi::edict_t *ent, const Vec3 &mins,
                 const Vec3 &maxs, ml::BoxHull &box_storage ) noexcept
{
    const EntityView view( ent );

    if ( view.solid() == abi::k_solid_bsp ||
         view.solid() == abi::k_solid_portal )
    {
        if ( view.solid() != abi::k_solid_portal &&
             view.movetype() != abi::k_movetype_push &&
             view.movetype() != abi::k_movetype_pushstep )
        {
            // Legacy: Host_Error "SOLID_BSP without MOVETYPE_PUSH..."
            ::xash::core::log( ::xash::core::LogLevel::Error, "server",
                               "SOLID_BSP without MOVETYPE_PUSH/PUSHSTEP" );
            return std::nullopt;
        }
        return hull_for_bsp_entity( env, ent, mins, maxs );
    }

    // create a temp hull from bounding box sizes (Minkowski expansion)
    SvHull out;
    out.hull   = box_storage.set_bounds( view.mins() - maxs,
                                         view.maxs() - mins );
    out.offset = view.origin();
    return out;
}

} // namespace xash::server
