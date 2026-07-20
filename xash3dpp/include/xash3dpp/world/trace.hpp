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
#include <xash3dpp/content/bone_solver.hpp> // StudioHitboxHull (OQ-2)
#include <xash3dpp/map_loader/trace.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/utilities/math.hpp>
#include <xash3dpp/utilities/matrix.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace xash::cmd_cvar { class CmdCvarContext; }

namespace xash::world {

// ---------------------------------------------------------------------------
// Seams
// ---------------------------------------------------------------------------

// Model identity by entvars modelindex.  The real model cache arrives with
// lifecycle (S7); tests provide fixture resolvers.  Brush submodels are
// submodel indices into the world's WorldData.
struct BrushModel
{
    std::size_t submodel   = 0;
    bool        has_origin = false; // legacy MODEL_HAS_ORIGIN (rotation support)
};

struct IModelResolver
{
    virtual ~IModelResolver() = default;

    [[nodiscard]] virtual std::optional<BrushModel>
    brush_model( int modelindex ) noexcept = 0;

    // studio-flagged entities take the OQ-2 studio-hull path when studio_bytes
    // yields a header; otherwise they fall back to the bbox hull exactly like
    // the legacy no-hitbox-data path.
    [[nodiscard]] virtual bool is_studio( int modelindex ) noexcept = 0;

    // Chunk 7: the studiohdr byte image for a studio model's modelindex, or an
    // empty span when the model is absent or not a studio model. The production
    // resolver lazily loads + caches the studio model (content::ModelCache);
    // callers wrap the span in a content::StudioView (pfnGetModelPtr, bone
    // position, attachment, the studio-hull provider). Default: none — test
    // fixtures and the bbox-fallback path do not resolve studio bytes.
    [[nodiscard]] virtual std::span<const std::byte> studio_bytes( int /*modelindex*/ ) noexcept
    {
        return {};
    }

    // OQ-2 studio hitbox hulls (Mod_HullForStudio's geometry+cache half).
    // `pose` is the fully-gated request the world/pm callers build
    // (SV_HullForStudioModel / PM_HullForStudio policy: size pre-scaled,
    // player blend applied, angles NOT yet pitch-flipped — the provider owns
    // the quake-bug flip, the 16-entry pose cache, and the CS shield skip).
    // Returns the hull count written to `out`, or 0 when the model has no
    // studio data or hitbox tracing is not enabled for it (header lacks
    // STUDIO_TRACE_HITBOX and pose.force_complex is false) — the caller then
    // takes the bbox fallback, exactly like the legacy NULL-hull path.
    [[nodiscard]] virtual int
    studio_hulls( int /*modelindex*/, const struct StudioHullPose & /*pose*/,
                  std::span<::xash::content::StudioHitboxHull> /*out*/ ) noexcept
    {
        return 0;
    }
};

// The gated studio-hull request (legacy SV_HullForStudioModel's locals).
// size carries the scale (0.5, or sv_clienttrace*0.5 with size (1,1,1));
// angles carry the player-blend pitch adjustment but NOT the quake-bug
// flip; controllers/blending are the entvars bytes or the client
// 0x7F×4 / {iBlend, 0} override; skip_shield mirrors `v.gamestate == 1`
// (CS shield); force_complex is the legacy useComplexHull outcome;
// use_cache gates the 16-entry pose cache (legacy mod_studiocache cvar).
struct StudioHullPose
{
    float                   frame    = 0.0f;
    int                     sequence = 0;
    ::xash::utilities::Vec3 angles{};
    ::xash::utilities::Vec3 origin{};
    ::xash::utilities::Vec3 size{};
    std::uint8_t            controllers[4]{};
    std::uint8_t            blending[2]{};
    bool                    skip_shield   = false;
    bool                    force_complex = false;
    bool                    use_cache     = true;
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
    ::xash::abi::edict_t           *ent      = nullptr; // @lifetime: arena (traced edict; non-owning)
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
    int             group_mask = 0; // legacy svs.groupmask (pfnSetGroupMask)
    bool quake_hull_select = false; // world FWORLD_SKYSPHERE (quake maps)
    bool quake_compat      = false; // host ENGINE_QUAKE_COMPATIBLE
    bool pusher_ext        = false; // host ENGINE_PHYSICS_PUSHER_EXT

    // OQ-2 studio hull gating inputs (read LIVE at trace time, matching the
    // legacy per-call cvar/global reads):
    ::xash::cmd_cvar::CmdCvarContext *cvars = nullptr; // @lifetime: engine (sv_clienttrace + mod_studiocache)
    const int *trace_flags = nullptr; // @lifetime: engine (svgame.globals->trace_flags; FTRACE_SIMPLEBOX)
};

// FTRACE_SIMPLEBOX (common/const.h:60): game requested plain-box tracing
// for this call — suppresses the zero-size complex-hull upgrade.
inline constexpr int k_ftrace_simplebox = 1 << 0;

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

// SV_StudioPlayerBlend (sv_world.c:78): map the view pitch into the
// sequence's blend window; adjusts *pitch in place and yields the 0..255
// blend byte. Pure — exposed for tests.
void studio_player_blend( const ::xash::content::SeqDescView &seq,
                          int *blend, float *pitch ) noexcept;

// SV_HullForStudioModel's GATING half (sv_world.c:281-349 up to the
// Mod_HullForStudio call): decides whether this entity takes the studio
// hitbox path and builds the pose request (size scaling, sv_clienttrace,
// player blend, CS shield flag). nullopt = take the bbox fallback.
// Exposed for tests.
[[nodiscard]] std::optional<StudioHullPose>
studio_pose_for_entity( const MoveEnv &env, ::xash::abi::edict_t *ent,
                        const ::xash::utilities::Vec3 &mins,
                        const ::xash::utilities::Vec3 &maxs ) noexcept;

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

// ---------------------------------------------------------------------------
// Point contents (world/contents.cpp)
// ---------------------------------------------------------------------------

// world.h RankForContents priority table (water < slime < lava < ... ).
[[nodiscard]] int rank_for_contents( int contents ) noexcept;

// SV_TruePointContents: world hull-0 contents merged with SOLID_NOT water
// bmodels from the areanode solid lists (highest rank wins; rotational
// water supported).
[[nodiscard]] int true_point_contents( const MoveEnv &env,
                                       const ::xash::utilities::Vec3 &p ) noexcept;

// SV_PointContents: CURRENT_* fold to CONTENTS_WATER.
[[nodiscard]] int point_contents( const MoveEnv &env,
                                  const ::xash::utilities::Vec3 &p ) noexcept;

// SV_TouchLinks' exact brush-trigger refinement (BSP hull forced at the
// TOUCHER's size, rotated triggers via MODEL_HAS_ORIGIN): true when the
// toucher's origin sits in the trigger's solid hull.  Lifecycle's
// IWorldLinkHooks implementation calls this (S7 wiring).
[[nodiscard]] bool brush_trigger_intersects( const MoveEnv &env,
                                             ::xash::abi::edict_t *trigger,
                                             ::xash::abi::edict_t *ent ) noexcept;

} // namespace xash::world
