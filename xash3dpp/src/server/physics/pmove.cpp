// xash3dpp — server player-move bridge: the entvars<->playermove_t state
// copy plus the physent/moveent/visent gather from the areanode tree
// (Chunk 6 pmove-bridge P2).
// Legacy reference: engine/server/sv_pmove.c — SV_CopyEdictToPhysEnt (:42),
// SV_AddLinksToPmove (:190), SV_AddLaddersToPmove (:282), SV_SetupPMove
// (:521), SV_FinishPMove (:599).
// Deep dive: docs/legacy-survey/deep-dive-server-physics.md §4.
//
// Q-20: this is the sanctioned raw `edict->v.` access site — the state copy is
// field-for-field with legacy (entity_view.hpp names the pmove bridge as the
// exception).  Deferrals (all marked inline):
//   * pe->model / pe->studiomodel are the opaque brush/studio handles the P3
//     trace family will bind; P2 leaves them null and classifies mins/maxs by
//     model type, which is all the gather membership needs.
//   * STUDIO_TRACE_HITBOX studiomodel selection needs studio extradata — the
//     Chunk 7 OQ-2 IStudioHullProvider; the bbox-hull fallback is correct now.
//   * Lag compensation (SV_GetTrueOrigin / the interpolant) is P5; the gather
//     uses un-interpolated positions.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/pmove.hpp>

#include <xash3dpp/abi/pm_defs.hpp>
#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/world/links.hpp>

#include <cstddef>

namespace xash::server {

namespace abi = ::xash::abi;
namespace ml  = ::xash::map_loader;

namespace {

// legacy angle indices (mathlib.h PITCH/YAW/ROLL).
inline constexpr int k_pitch = 0;
inline constexpr int k_yaw   = 1;
inline constexpr int k_roll  = 2;

inline void copy_vec3( abi::vec3_t dst, const abi::vec3_t src ) noexcept
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// compliance-allow(thread-assert): pure inline helper zeroing a
// caller-supplied vec3, no shared state
inline void clear_vec3( abi::vec3_t v ) noexcept
{
    v[0] = v[1] = v[2] = 0.0f;
}

[[nodiscard]] inline bool vec3_is_null( const abi::vec3_t v ) noexcept
{
    return v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f;
}

[[nodiscard]] inline bool fbit( int flags, int bit ) noexcept
{
    return ( flags & bit ) != 0;
}

// legacy BoundsIntersect (world.h): strict-inequality reject per axis.
[[nodiscard]] bool bounds_intersect( const abi::vec3_t mins1,
                                     const abi::vec3_t maxs1,
                                     const abi::vec3_t mins2,
                                     const abi::vec3_t maxs2 ) noexcept
{
    for ( int i = 0; i < 3; ++i )
    {
        if ( mins1[i] > maxs2[i] || maxs1[i] < mins2[i] )
            return false;
    }
    return true;
}

// Bounded C-string copy (legacy Q_strncpy — always NUL-terminates).
void copy_cstr( char *dst, const char *src, std::size_t cap ) noexcept
{
    if ( cap == 0 )
        return;
    std::size_t i = 0;
    if ( src != nullptr )
        for ( ; src[i] != '\0' && i + 1 < cap; ++i )
            dst[i] = src[i];
    dst[i] = '\0';
}

// SV_ModelHandle equivalent: a physent needs a real precached model.  Legacy
// returns NULL for modelindex 0 / unprecached slots and the entity is skipped;
// the C++ resolver's is_studio() answers "not a brush" even for those, so the
// precache-slot name is the authoritative "has a model" test.
[[nodiscard]] bool has_model( ServerRuntime &rt, int modelindex ) noexcept
{
    if ( modelindex <= 0 )
        return false;
    if ( modelindex == abi::k_world_index )
        return true;
    return rt.precache.model_name(
               static_cast<std::size_t>( modelindex ))[0] != '\0';
}

// ---------------------------------------------------------------------------
// SV_CopyEdictToPhysEnt (sv_pmove.c:42)
// ---------------------------------------------------------------------------

[[nodiscard]] bool copy_edict_to_physent( ServerRuntime &rt, abi::physent_t *pe,
                                          abi::edict_t *ed ) noexcept
{
    const int modelindex = ed->v.modelindex;
    if ( !has_model( rt, modelindex ))
        return false;

    const bool is_brush  = rt.models.brush_model( modelindex ).has_value();
    const bool is_studio = rt.models.is_studio( modelindex );

    pe->player = 0;
    pe->info   = rt.arena.index_of( ed );
    copy_vec3( pe->origin, ed->v.origin );
    copy_vec3( pe->angles, ed->v.angles );

    if ( fbit( ed->v.flags, abi::k_fl_client ))
    {
        // client — lag-comp GetTrueOrigin is P5 (un-interpolated here).
        if ( fbit( ed->v.flags, abi::k_fl_fakeclient ))
            copy_cstr( pe->name, "bot", sizeof( pe->name ));
        else
            copy_cstr( pe->name, "player", sizeof( pe->name ));
        pe->player = pe->info;
    }
    else
    {
        copy_cstr( pe->name,
                   rt.precache.model_name(
                       static_cast<std::size_t>( modelindex )),
                   sizeof( pe->name ));
    }

    // pe->model / pe->studiomodel stay null by design: the P3 trace family
    // (pm_trace.cpp) resolves a physent's brush submodel through the arena +
    // IModelResolver (pe->info -> edict -> modelindex), not an opaque engine
    // handle, so nothing needs to be stashed here.  The model type still
    // selects mins/maxs below.
    pe->model = pe->studiomodel = nullptr;

    switch ( ed->v.solid )
    {
    case abi::k_solid_not:
    case abi::k_solid_bsp:
        // brush model → engine hull, no bbox extents.
        clear_vec3( pe->mins );
        clear_vec3( pe->maxs );
        break;
    case abi::k_solid_bbox:
        // OQ-2 (resolved 2026-07-19): legacy stashes pe->studiomodel here for
        // hitbox tracing; xash3dpp keeps the handle null BY DESIGN — the
        // trace family resolves studio hulls at trace time via
        // pe->info -> arena -> modelindex -> IModelResolver::studio_hulls
        // (pm_trace.cpp pm_studio_hulls), so nothing needs stashing. The
        // physent already carries the full pose (frame/sequence/angles/
        // origin/controller/blending, copied below).
        (void)is_studio;
        copy_vec3( pe->mins, ed->v.mins );
        copy_vec3( pe->maxs, ed->v.maxs );
        break;
    case abi::k_solid_custom:
        // handles stay null (resolved via the arena at trace time — see above);
        // pm_trace.cpp routes SOLID_CUSTOM to the S8 physics-interface sweep.
        copy_vec3( pe->mins, ed->v.mins );
        copy_vec3( pe->maxs, ed->v.maxs );
        break;
    default:
        // studio handle stays null → the trace family takes the bbox fallback
        // until the Chunk 7 hitbox provider (OQ-2).
        copy_vec3( pe->mins, ed->v.mins );
        copy_vec3( pe->maxs, ed->v.maxs );
        break;
    }
    (void)is_brush;

    pe->solid      = ed->v.solid;
    pe->rendermode = ed->v.rendermode;
    pe->skin       = ed->v.skin;
    pe->frame      = ed->v.frame;
    pe->sequence   = ed->v.sequence;

    for ( int i = 0; i < 4; ++i )
        pe->controller[i] = ed->v.controller[i];
    pe->blending[0] = ed->v.blending[0];
    pe->blending[1] = ed->v.blending[1];

    pe->movetype    = ed->v.movetype;
    pe->takedamage  = static_cast<int>( ed->v.takedamage );
    pe->team        = ed->v.team;
    pe->classnumber = ed->v.playerclass;
    pe->blooddecal  = 0; // unused in GoldSrc

    pe->iuser1 = ed->v.iuser1;
    pe->iuser2 = ed->v.iuser2;
    pe->iuser3 = ed->v.iuser3;
    pe->iuser4 = ed->v.iuser4;
    pe->fuser1 = ed->v.fuser1;
    pe->fuser2 = ed->v.fuser2;
    pe->fuser3 = ed->v.fuser3;
    pe->fuser4 = ed->v.fuser4;
    copy_vec3( pe->vuser1, ed->v.vuser1 );
    copy_vec3( pe->vuser2, ed->v.vuser2 );
    copy_vec3( pe->vuser3, ed->v.vuser3 );
    copy_vec3( pe->vuser4, ed->v.vuser4 );

    return true;
}

// ---------------------------------------------------------------------------
// SV_AddLinksToPmove (sv_pmove.c:190) — collect solid + visible entities
// ---------------------------------------------------------------------------

// compliance-allow(thread-assert): TU-private recursive area-node walk
// filling the caller's playermove_t; sole entry sv_setup_pmove
// (pmove.cpp:351) asserts Main
void add_links_to_pmove( ServerRuntime &rt, abi::playermove_t &pm,
                         const AreaNode *node, abi::edict_t *pl,
                         const abi::vec3_t pmove_mins,
                         const abi::vec3_t pmove_maxs ) noexcept
{
    const abi::link_t *stop = &node->solid_edicts;
    for ( const abi::link_t *l = node->solid_edicts.next; l != stop; )
    {
        abi::edict_t *check = edict_from_area( const_cast<abi::link_t *>( l )); // SAFETY: Q-16 const_cast — the area list is traversed read-only, but edict_from_area recovers the OWNING (mutable) edict via offsetof back-cast; the links thread the mutable arena edicts
        l = l->next;

        if ( check->v.groupinfo != 0 )
        {
            if ( rt.move_env.group_op == GroupOp::And &&
                 !fbit( check->v.groupinfo, pl->v.groupinfo ))
                continue;
            if ( rt.move_env.group_op == GroupOp::Nand &&
                 fbit( check->v.groupinfo, pl->v.groupinfo ))
                continue;
        }

        if ( check->v.owner == pl || check->v.solid == abi::k_solid_trigger )
            continue; // player or player's own missile

        if ( pm.numvisent < abi::k_max_physents )
        {
            abi::physent_t *pe = &pm.visents[pm.numvisent];
            if ( copy_edict_to_physent( rt, pe, check ))
                pm.numvisent++;
        }

        if ( check->v.solid == abi::k_solid_not &&
             ( check->v.skin == ml::k_contents_none ||
               check->v.modelindex == 0 ))
            continue;

        // ignore monsterclip brushes
        if ( fbit( check->v.flags, abi::k_fl_monsterclip ) &&
             check->v.solid == abi::k_solid_bsp )
            continue;

        if ( check == pl )
            continue; // himself

        // nehahra collision flags — skip dead bodies (non-pushers only)
        if ( check->v.movetype != abi::k_movetype_push )
        {
            if (( fbit( check->v.flags, abi::k_fl_client | abi::k_fl_fakeclient ) &&
                  check->v.health <= 0.0f ) ||
                check->v.deadflag == abi::k_dead_dead )
                continue;
        }

        if ( vec3_is_null( check->v.size ))
            continue;

        // FL_CLIENT interpolated absmin/absmax (SV_GetTrueMinMax) is P5.
        if ( !bounds_intersect( pmove_mins, pmove_maxs, check->v.absmin,
                                check->v.absmax ))
            continue;

        if ( pm.numphysent < abi::k_max_physents )
        {
            abi::physent_t *pe = &pm.physents[pm.numphysent];
            if ( copy_edict_to_physent( rt, pe, check ))
                pm.numphysent++;
        }
    }

    // recurse down both sides
    if ( node->axis == -1 )
        return;
    if ( pmove_maxs[node->axis] > node->dist )
        add_links_to_pmove( rt, pm, node->children[0], pl, pmove_mins, pmove_maxs );
    if ( pmove_mins[node->axis] < node->dist )
        add_links_to_pmove( rt, pm, node->children[1], pl, pmove_mins, pmove_maxs );
}

// ---------------------------------------------------------------------------
// SV_AddLaddersToPmove (sv_pmove.c:282)
// ---------------------------------------------------------------------------

// compliance-allow(thread-assert): TU-private recursive area-node walk
// filling the caller's playermove_t; sole entry sv_setup_pmove
// (pmove.cpp:351) asserts Main
void add_ladders_to_pmove( ServerRuntime &rt, abi::playermove_t &pm,
                           const AreaNode *node, const abi::vec3_t pmove_mins,
                           const abi::vec3_t pmove_maxs ) noexcept
{
    const abi::link_t *stop = &node->solid_edicts;
    for ( const abi::link_t *l = node->solid_edicts.next; l != stop; )
    {
        abi::edict_t *check = edict_from_area( const_cast<abi::link_t *>( l )); // SAFETY: Q-16 const_cast — the area list is traversed read-only, but edict_from_area recovers the OWNING (mutable) edict via offsetof back-cast; the links thread the mutable arena edicts
        l = l->next;

        if ( check->v.solid != abi::k_solid_not ||
             check->v.skin != ml::k_contents_ladder )
            continue;

        // only brushes can have special contents
        if ( !rt.models.brush_model( check->v.modelindex ).has_value() )
            continue;

        if ( !bounds_intersect( pmove_mins, pmove_maxs, check->v.absmin,
                                check->v.absmax ))
            continue;

        if ( pm.nummoveent == abi::k_max_moveents )
            return;

        abi::physent_t *pe = &pm.moveents[pm.nummoveent];
        if ( copy_edict_to_physent( rt, pe, check ))
            pm.nummoveent++;
    }

    if ( node->axis == -1 )
        return;
    if ( pmove_maxs[node->axis] > node->dist )
        add_ladders_to_pmove( rt, pm, node->children[0], pmove_mins, pmove_maxs );
    if ( pmove_mins[node->axis] < node->dist )
        add_ladders_to_pmove( rt, pm, node->children[1], pmove_mins, pmove_maxs );
}

} // namespace

// ---------------------------------------------------------------------------
// SV_SetupPMove (sv_pmove.c:521)
// ---------------------------------------------------------------------------

void sv_setup_pmove( ServerRuntime &rt, ServerClient &cl,
                     const abi::usercmd_t &ucmd, const char *physinfo ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    if ( rt.pmove == nullptr || cl.edict == nullptr )
        return;

    abi::playermove_t &pm    = *rt.pmove;
    abi::edict_t      *clent = cl.edict;

    rt.globals.frametime = ( ucmd.msec * 0.001f );

    pm.player_index = rt.arena.index_of( clent ) - 1;
    pm.multiplayer  = ( rt.clients.maxclients > 1 ) ? 1 : 0;
    pm.time         = static_cast<float>( cl.timebase * 1000.0 );
    copy_vec3( pm.origin, clent->v.origin );
    copy_vec3( pm.angles, clent->v.v_angle );
    copy_vec3( pm.oldangles, clent->v.v_angle );
    copy_vec3( pm.velocity, clent->v.velocity );
    copy_vec3( pm.basevelocity, clent->v.basevelocity );
    copy_vec3( pm.view_ofs, clent->v.view_ofs );
    copy_vec3( pm.movedir, clent->v.movedir );
    pm.flDuckTime      = clent->v.flDuckTime;
    pm.bInDuck         = clent->v.bInDuck;
    pm.usehull         = fbit( clent->v.flags, abi::k_fl_ducking ) ? 1 : 0;
    pm.flTimeStepSound = clent->v.flTimeStepSound;
    pm.iStepLeft       = clent->v.iStepLeft;
    pm.flFallVelocity  = clent->v.flFallVelocity;
    pm.flSwimTime      = clent->v.flSwimTime;
    copy_vec3( pm.punchangle, clent->v.punchangle );
    pm.effects       = clent->v.effects;
    pm.flags         = clent->v.flags;
    pm.gravity       = clent->v.gravity;
    pm.friction      = clent->v.friction;
    pm.oldbuttons    = clent->v.oldbuttons;
    pm.waterjumptime = clent->v.teleport_time;
    pm.dead          = ( clent->v.health <= 0.0f ) ? 1 : 0;
    pm.deadflag      = clent->v.deadflag;
    pm.spectator     = 0; // spectator physics all run on the client
    pm.movetype      = clent->v.movetype;
    if ( pm.multiplayer )
        pm.onground = -1;
    pm.waterlevel     = clent->v.waterlevel;
    pm.watertype      = clent->v.watertype;
    pm.maxspeed       = rt.movevars.maxspeed;
    pm.clientmaxspeed = clent->v.maxspeed;
    pm.iuser1         = clent->v.iuser1;
    pm.iuser2         = clent->v.iuser2;
    pm.iuser3         = clent->v.iuser3;
    pm.iuser4         = clent->v.iuser4;
    pm.fuser1         = clent->v.fuser1;
    pm.fuser2         = clent->v.fuser2;
    pm.fuser3         = clent->v.fuser3;
    pm.fuser4         = clent->v.fuser4;
    copy_vec3( pm.vuser1, clent->v.vuser1 );
    copy_vec3( pm.vuser2, clent->v.vuser2 );
    copy_vec3( pm.vuser3, clent->v.vuser3 );
    copy_vec3( pm.vuser4, clent->v.vuser4 );
    pm.cmd      = ucmd; // setup current cmds
    pm.runfuncs = 1;

    copy_cstr( pm.physinfo, physinfo, sizeof( pm.physinfo ));

    // setup physents
    pm.numvisent  = 0;
    pm.numphysent = 0;
    pm.nummoveent = 0;

    abi::vec3_t absmin, absmax;
    for ( int i = 0; i < 3; ++i )
    {
        absmin[i] = clent->v.origin[i] - 256.0f;
        absmax[i] = clent->v.origin[i] + 256.0f;
    }

    // always start with the world (edict 0)
    (void)copy_edict_to_physent( rt, &pm.physents[0], rt.arena.edict_num( 0 ));
    pm.visents[0]  = pm.physents[0];
    pm.numphysent  = 1;
    pm.numvisent   = 1;

    const AreaNode *root = rt.links.root();
    if ( root != nullptr )
    {
        add_links_to_pmove( rt, pm, root, clent, absmin, absmax );
        add_ladders_to_pmove( rt, pm, root, absmin, absmax );
    }
}

// ---------------------------------------------------------------------------
// SV_FinishPMove (sv_pmove.c:599)
// ---------------------------------------------------------------------------

void sv_finish_pmove( ServerRuntime &rt, ServerClient &cl ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    if ( rt.pmove == nullptr || cl.edict == nullptr )
        return;

    abi::playermove_t &pm    = *rt.pmove;
    abi::edict_t      *clent = cl.edict;

    clent->v.teleport_time = pm.waterjumptime;
    copy_vec3( clent->v.origin, pm.origin );
    copy_vec3( clent->v.view_ofs, pm.view_ofs );
    copy_vec3( clent->v.velocity, pm.velocity );
    copy_vec3( clent->v.basevelocity, pm.basevelocity );
    copy_vec3( clent->v.punchangle, pm.punchangle );
    copy_vec3( clent->v.movedir, pm.movedir );
    clent->v.flTimeStepSound = pm.flTimeStepSound;
    clent->v.flFallVelocity  = pm.flFallVelocity;
    clent->v.oldbuttons      = static_cast<int>( pm.cmd.buttons );
    clent->v.waterlevel      = pm.waterlevel;
    clent->v.watertype       = pm.watertype;
    clent->v.maxspeed        = pm.clientmaxspeed;
    clent->v.flDuckTime      = static_cast<int>( pm.flDuckTime );
    clent->v.flSwimTime      = static_cast<int>( pm.flSwimTime );
    clent->v.iStepLeft       = pm.iStepLeft;
    clent->v.movetype        = pm.movetype;
    clent->v.friction        = pm.friction;
    clent->v.deadflag        = pm.deadflag;
    clent->v.effects         = pm.effects;
    clent->v.bInDuck         = pm.bInDuck;
    clent->v.flags           = pm.flags;

    clent->v.iuser1 = pm.iuser1;
    clent->v.iuser2 = pm.iuser2;
    clent->v.iuser3 = pm.iuser3;
    clent->v.iuser4 = pm.iuser4;
    clent->v.fuser1 = pm.fuser1;
    clent->v.fuser2 = pm.fuser2;
    clent->v.fuser3 = pm.fuser3;
    clent->v.fuser4 = pm.fuser4;
    copy_vec3( clent->v.vuser1, pm.vuser1 );
    copy_vec3( clent->v.vuser2, pm.vuser2 );
    copy_vec3( clent->v.vuser3, pm.vuser3 );
    copy_vec3( clent->v.vuser4, pm.vuser4 );

    if ( pm.onground == -1 )
    {
        clent->v.flags &= ~abi::k_fl_onground;
    }
    else if ( pm.onground >= 0 && pm.onground < pm.numphysent )
    {
        clent->v.flags |= abi::k_fl_onground;
        clent->v.groundentity =
            rt.arena.edict_num(
                static_cast<std::size_t>( pm.physents[pm.onground].info ));
    }

    // angles — show 1/3 the pitch angle and all the roll angle
    if ( !clent->v.fixangle )
    {
        copy_vec3( clent->v.v_angle, pm.angles );
        clent->v.angles[k_pitch] = -( clent->v.v_angle[k_pitch] / 3.0f );
        clent->v.angles[k_roll]  = clent->v.v_angle[k_roll];
        clent->v.angles[k_yaw]   = clent->v.v_angle[k_yaw];
    }

    // SV_SetMinMaxSize( clent, host.player_mins[usehull], ..., false ):
    // relink=false, so just store the hull extents + size (no area re-link).
    const int hull = ( pm.usehull >= 0 && pm.usehull < 4 ) ? pm.usehull : 0;
    const auto &hb = rt.hull_bounds[static_cast<std::size_t>( hull )];
    clent->v.mins[0] = hb.mins.x;
    clent->v.mins[1] = hb.mins.y;
    clent->v.mins[2] = hb.mins.z;
    clent->v.maxs[0] = hb.maxs.x;
    clent->v.maxs[1] = hb.maxs.y;
    clent->v.maxs[2] = hb.maxs.z;
    for ( int i = 0; i < 3; ++i )
        clent->v.size[i] = clent->v.maxs[i] - clent->v.mins[i];

    // all next calls ignore footstep sounds
    pm.runfuncs = 0;
}

void pm_clear_phys_ents( ServerRuntime &rt ) noexcept
{
    if ( rt.pmove == nullptr )
        return;
    rt.pmove->numphysent = 0;
    rt.pmove->numvisent  = 0;
    rt.pmove->nummoveent = 0;
}

} // namespace xash::server
