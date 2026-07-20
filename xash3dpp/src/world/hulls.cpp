// xash3dpp — server hull selection (Chunk 6 S5b)
// Legacy reference: engine/server/sv_world.c :152-273
//
// Existing subsystems used:
//   xash3dpp_map_loader — world_hull views + BoxHull
//   xash3dpp_core       — logging for the legacy Host_Error conditions

#include <xash3dpp/world/trace.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/content/studio.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/abi/entity_view.hpp>

namespace xash::world {

namespace ml  = ::xash::map_loader;
namespace abi = ::xash::abi;
namespace ut  = ::xash::utilities;

// Shared symbols now live in their homes (Wave 1a); import those spelled bare.
using ut::Vec3;
using abi::EntityView;

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

namespace {

// Cvar read with the LEGACY REGISTERED DEFAULT when the cvar is absent —
// cvar_variable_value returns 0 for unregistered names, which would silently
// flip sv_clienttrace/mod_studiocache to their non-default (off) behaviour.
[[nodiscard]] float cvar_or_default( ::xash::cmd_cvar::CmdCvarContext *cvars,
                                     const char *name, float def ) noexcept
{
    if ( cvars == nullptr )
        return def;
    const ::xash::cmd_cvar::Cvar *cv = cvars->cvar_find( name );
    return cv != nullptr ? cv->abi.value : def;
}

} // namespace

// ---------------------------------------------------------------------------
// SV_StudioPlayerBlend (sv_world.c:78)
// ---------------------------------------------------------------------------

void studio_player_blend( const ::xash::content::SeqDescView &seq,
                          int *blend, float *pitch ) noexcept
{
    const float blend_start = seq.blend_start0();
    const float blend_end   = seq.blend_end0();

    // calc up/down pointing
    *blend = static_cast<int>( *pitch * 3.0f );

    if ( static_cast<float>( *blend ) < blend_start )
    {
        *pitch -= blend_start / 3.0f;
        *blend  = 0;
    }
    else if ( static_cast<float>( *blend ) > blend_end )
    {
        *pitch -= blend_end / 3.0f;
        *blend  = 255;
    }
    else
    {
        if ( blend_end - blend_start < 0.1f ) // catch qc error
            *blend = 127;
        else
            *blend = static_cast<int>(
                255.0f * ( static_cast<float>( *blend ) - blend_start ) /
                ( blend_end - blend_start ));
        *pitch = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// SV_HullForStudioModel — the gating half (sv_world.c:281-349)
// ---------------------------------------------------------------------------

std::optional<StudioHullPose>
studio_pose_for_entity( const MoveEnv &env, abi::edict_t *ent,
                        const ::xash::utilities::Vec3 &mins,
                        const ::xash::utilities::Vec3 &maxs ) noexcept
{
    if ( env.models == nullptr )
        return std::nullopt;

    const EntityView view( ent );
    const ::xash::utilities::Vec3 raw_size = maxs - mins;

    float scale         = 0.5f;
    bool  use_complex   = false;
    ::xash::utilities::Vec3 size = raw_size;

    const bool simplebox =
        env.trace_flags != nullptr &&
        ( *env.trace_flags & k_ftrace_simplebox ) != 0;
    const bool is_client =
        ( view.flags() & ( abi::k_fl_client | abi::k_fl_fakeclient )) != 0;

    if ( vector_is_null( raw_size ) && !simplebox )
    {
        use_complex = true;

        if ( is_client )
        {
            // sv_clienttrace: 0 = no hitbox tracing for clients (bbox);
            // otherwise it scales the point-trace test box. Registered
            // default "1" (sv_main.c) when the cvar is absent.
            const float clienttrace =
                cvar_or_default( env.cvars, "sv_clienttrace", 1.0f );
            if ( clienttrace == 0.0f )
            {
                use_complex = false;
            }
            else
            {
                scale = clienttrace * 0.5f;
                size  = { 1.0f, 1.0f, 1.0f };
            }
        }
    }

    // The STUDIO_TRACE_HITBOX force-gate lives with the provider (it owns
    // the header); a pose with force_complex=false still reaches it so the
    // flag can force the hitbox path for sized boxes (legacy OR-condition).
    StudioHullPose pose;
    pose.frame         = view.frame();
    pose.sequence      = view.sequence();
    pose.origin        = view.origin();
    pose.angles        = view.angles();
    pose.size          = size * scale;      // VectorScale(size, scale, size)
    pose.skip_shield   = view.gamestate() == 1; // CS shield (Mod_HullForStudio)
    pose.force_complex = use_complex;
    // Console name "r_studiocache" — the legacy C identifier is
    // mod_studiocache (model.c:31 CVAR_DEFINE).
    pose.use_cache =
        cvar_or_default( env.cvars, "r_studiocache", 1.0f ) != 0.0f;

    const auto ctrl  = view.controller();
    const auto blend = view.blending();
    for ( int i = 0; i < 4; ++i )
        pose.controllers[i] = ctrl[static_cast<std::size_t>( i )];
    pose.blending[0] = blend[0];
    pose.blending[1] = blend[1];

    if ( is_client )
    {
        // Client pose override: neutral controllers + pitch-derived blend
        // (SV_StudioPlayerBlend over the sequence's blend window).
        const auto bytes = env.models->studio_bytes( view.modelindex() );
        if ( !bytes.empty() )
        {
            const ::xash::content::StudioView hdr( bytes );
            const auto seq = hdr.seqdesc( view.sequence() );
            int   iblend = 0;
            float pitch  = pose.angles.x;
            studio_player_blend( seq, &iblend, &pitch );
            pose.angles.x = pitch;

            pose.controllers[0] = pose.controllers[1] = 0x7F;
            pose.controllers[2] = pose.controllers[3] = 0x7F;
            pose.blending[0]    = static_cast<std::uint8_t>( iblend );
            pose.blending[1]    = 0;
        }
    }

    return pose;
}

} // namespace xash::world
