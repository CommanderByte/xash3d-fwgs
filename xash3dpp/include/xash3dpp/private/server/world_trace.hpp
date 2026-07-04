#pragma once
// xash3dpp — server trace composition: hull selection + entity clipping
// Legacy reference: engine/server/sv_world.c — SV_HullForBox (:152),
// SV_HullForBsp (:176), SV_HullForEntity (:248), SV_ClipMoveToEntity
// (:836), SV_ClipToEntity/SV_ClipToLinks/SV_ClipToPortals (:1106-1260),
// SV_ClipToWorldBrush (:1270), SV_Move (:1314), SV_MoveNoEnts (:1368),
// SV_CheckSphereIntersection (:109); engine/common/world.h —
// World_MoveBounds/World_CombineTraces/check_angles;
// engine/common/world.c — World_TransformAABB (:29);
// public/matrixlib.c — Matrix4x4_TransformPositivePlane (:397).
// Deep dive: docs/legacy-survey/deep-dive-server-world-frame.md §3.
//
// Composes the map_loader trace kernel (edict-free by contract) over the
// edict store: per-entity hull selection, rotated-brush transforms, the
// 15-step clip filter chain, and the SV_Move fraction-rescale quirk.
// Entvars access via EntityView (Q-20); edict identity stays edict_t*.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/edict.hpp>
#include <xash3dpp/map_loader/trace.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/world_links.hpp>
#include <xash3dpp/utilities/math.hpp>
#include <xash3dpp/utilities/matrix.hpp>

#include <optional>

namespace xash::server {

// ---------------------------------------------------------------------------
// Seams
// ---------------------------------------------------------------------------

// Model identity by entvars modelindex.  The real model cache arrives with
// lifecycle (S7); tests provide fixture resolvers.  Brush submodels are
// submodel indices into the world's WorldData.
struct BrushModel
{
    std::size_t submodel = 0;
};

struct IModelResolver
{
    virtual ~IModelResolver() = default;

    [[nodiscard]] virtual std::optional<BrushModel>
    brush_model( int modelindex ) noexcept = 0;

    // TODO(chunk7): studio models take the hitbox path via the OQ-2
    // IStudioHullProvider; until then studio-flagged entities fall back
    // to the bbox hull exactly like the legacy no-hitbox-data path.
    [[nodiscard]] virtual bool is_studio( int modelindex ) noexcept = 0;
};

struct SvTrace; // fwd

// Game/physics hooks consulted by the clip filter chain; the bridge (S6)
// and the physics interface (S8) install the real ones.
struct IClipHooks
{
    virtual ~IClipHooks() = default;

    // NEW_DLL_FUNCTIONS::pfnShouldCollide — absent hook collides.
    [[nodiscard]] virtual bool
    should_collide( ::xash::abi::edict_t * /*touch*/,
                    ::xash::abi::edict_t * /*pass*/ ) noexcept
    {
        return true;
    }

    // SV_CheckSphereIntersection — player sequence-bbox sphere cull;
    // needs studio extradata (S7 model cache), legacy passes when absent.
    [[nodiscard]] virtual bool
    sphere_cull( ::xash::abi::edict_t * /*touch*/,
                 const ::xash::utilities::Vec3 & /*start*/,
                 const ::xash::utilities::Vec3 & /*end*/ ) noexcept
    {
        return true;
    }

    // SOLID_CUSTOM → physics-interface ClipMoveToEntity; the legacy
    // missing-hook default is a no-hit trace with allsolid cleared.
    virtual void custom_clip( ::xash::abi::edict_t *touch,
                              const ::xash::utilities::Vec3 &start,
                              const ::xash::utilities::Vec3 &mins,
                              const ::xash::utilities::Vec3 &maxs,
                              const ::xash::utilities::Vec3 &end,
                              SvTrace &out ) noexcept;
};

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

// Legacy trace_t at the engine-internal layer: kernel result + hit entity
// + studio hitgroup.  The ABI trace_t projection happens at the bridge.
struct SvTrace
{
    ::xash::map_loader::TraceResult t{};
    ::xash::abi::edict_t           *ent      = nullptr;
    int                             hitgroup = 0;
};

// Per-call environment (server state the walks need; lifecycle owns it).
struct MoveEnv
{
    const ::xash::map_loader::WorldData *world = nullptr; // @lifetime: engine
    IModelResolver *models     = nullptr;                 // @lifetime: engine
    AreaNode       *area_root  = nullptr;                 // @lifetime: engine
    ::xash::abi::edict_t *worldspawn = nullptr;           // @lifetime: engine
    IClipHooks     *hooks      = nullptr;                 // @lifetime: engine (null = defaults)
    GroupOp         group_op   = GroupOp::And;
    bool quake_hull_select = false; // world FWORLD_SKYSPHERE (quake maps)
    bool quake_compat      = false; // host ENGINE_QUAKE_COMPATIBLE
    bool pusher_ext        = false; // host ENGINE_PHYSICS_PUSHER_EXT
};

// SV_HullForBsp/SV_HullForEntity result: hull view + the offset added to
// the tested object's origin to reach hull-local space.
struct SvHull
{
    ::xash::map_loader::TraceHull hull{};
    ::xash::utilities::Vec3       offset{};
};

// ---------------------------------------------------------------------------
// Hull selection (world/hulls.cpp)
// ---------------------------------------------------------------------------

// SV_HullForBsp: size-based hull select (Quake vs HL rules, the hull-0
// verbatim-clip_mins point quirk) + origin offset.  nullopt = the legacy
// Host_Error conditions (non-brush model) — logged, mapped to the host
// error policy at the bridge (Q-5; Known Deviation).
[[nodiscard]] std::optional<SvHull>
hull_for_bsp_entity( const MoveEnv &env, ::xash::abi::edict_t *ent,
                     const ::xash::utilities::Vec3 &mins,
                     const ::xash::utilities::Vec3 &maxs ) noexcept;

// SV_HullForEntity: BSP/portal solids route to hull_for_bsp_entity (with
// the MOVETYPE_PUSH/PUSHSTEP guard); everything else is a Minkowski box
// built into caller-owned storage.
[[nodiscard]] std::optional<SvHull>
hull_for_entity( const MoveEnv &env, ::xash::abi::edict_t *ent,
                 const ::xash::utilities::Vec3 &mins,
                 const ::xash::utilities::Vec3 &maxs,
                 ::xash::map_loader::BoxHull &box_storage ) noexcept;

// ---------------------------------------------------------------------------
// Transforms (world/clip.cpp; exposed for tests)
// ---------------------------------------------------------------------------

// World_TransformAABB (rotation-only corner sweep through the inverse).
void world_transform_aabb( const ::xash::utilities::Matrix3x4 &m,
                           const ::xash::utilities::Vec3 &mins,
                           const ::xash::utilities::Vec3 &maxs,
                           ::xash::utilities::Vec3 &out_mins,
                           ::xash::utilities::Vec3 &out_maxs ) noexcept;

// Matrix4x4_TransformPositivePlane at scale 1.
void transform_positive_plane( const ::xash::utilities::Matrix3x4 &m,
                               const ::xash::map_loader::TracePlane &in,
                               ::xash::map_loader::TracePlane &out ) noexcept;

// ---------------------------------------------------------------------------
// Clipping (world/clip.cpp)
// ---------------------------------------------------------------------------

// SV_ClipMoveToEntity: hull select (studio → bbox fallback until Chunk 7),
// rotated-brush frames (incl. the ENGINE_PHYSICS_PUSHER_EXT transform_bbox
// path with its sign-dependent offset quirk), kernel invocation, endpos
// lerp + plane recompute, hit-entity stamp.
[[nodiscard]] SvTrace
clip_move_to_entity( const MoveEnv &env, ::xash::abi::edict_t *ent,
                     const ::xash::utilities::Vec3 &start,
                     const ::xash::utilities::Vec3 &mins,
                     const ::xash::utilities::Vec3 &maxs,
                     const ::xash::utilities::Vec3 &end ) noexcept;

// SV_Move: world clip first, then (when fraction != 0) the entity pass
// over the areanode solid+portal lists against the world-clipped segment,
// with the fraction-compose quirk (entity fraction × world fraction).
// `type` low byte = MOVE_*, high byte = ignore-transparent.
[[nodiscard]] SvTrace
move( const MoveEnv &env, const ::xash::utilities::Vec3 &start,
      const ::xash::utilities::Vec3 &mins,
      const ::xash::utilities::Vec3 &maxs,
      const ::xash::utilities::Vec3 &end, int type,
      ::xash::abi::edict_t *passedict, bool monsterclip ) noexcept;

// SV_MoveNoEnts: entity pass replaced by FL_WORLDBRUSH SOLID_BSP ents.
[[nodiscard]] SvTrace
move_no_ents( const MoveEnv &env, const ::xash::utilities::Vec3 &start,
              const ::xash::utilities::Vec3 &mins,
              const ::xash::utilities::Vec3 &maxs,
              const ::xash::utilities::Vec3 &end, int type,
              ::xash::abi::edict_t *passedict ) noexcept;

} // namespace xash::server
