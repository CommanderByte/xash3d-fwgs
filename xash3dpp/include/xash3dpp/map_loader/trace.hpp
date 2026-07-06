#pragma once
// xash3dpp — clip-hull trace kernel and hull selection.
// Legacy reference: engine/common/pm_trace.c — PM_HullPointContents (:113),
// PM_RecursiveHullCheck (:200-323, THE kernel), PM_InitBoxHull/PM_HullForBox
// (:64-105), PM_HullForBsp (:146-176), PM_PlayerTraceExt post-processing
// (:504-529); engine/common/mod_bmodel.c box_clipnodes (:567-594).
// Deep dive: docs/legacy-survey/deep-dive-trace-pvs.md §1.
//
// This is the ONE canonical trace kernel: legacy server physics (SV_Move),
// the pfnPM_Move seam and client prediction all funnel through
// PM_RecursiveHullCheck.  Q-18 applies in full — float-for-float parity,
// strict FP, golden trace fixtures gate this file.
//
// DELIBERATELY EDICT-FREE (hard Chunk 6 prerequisite): no physent list, no
// entity indices, no usehull global.  The server composes per-entity traces
// from hull_for_bsp/BoxHull + trace_hull + finalize_trace and owns
// nearest-fraction merging and hit-entity recording.
//
// Tie-break note (opposite of the PVS point_leaf walk): an exactly-on-plane
// point goes to the FRONT child here (PlaneDiff < 0 selects the back).
//
// @thread-safety: trace queries are concurrent-read-safe over a const WorldData; BoxHull is a per-instance value type (one per thread/callsite — no shared static like the legacy pm_boxhull).

#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace xash::map_loader {

inline constexpr float k_dist_epsilon = 1.0f / 32.0f; // legacy DIST_EPSILON

// Non-owning hull_t equivalent.  For world hulls, `clipnodes` is
// hull0_nodes() (hull 0) or clipnodes() (hulls 1-3) and `planes` is
// planes(); an absent hull carries empty spans (legacy planes == NULL:
// point contents answer CONTENTS_NONE, traces treat the hull as open).
// Pre: clipnode planenum/child indices are in range (guaranteed for hulls
// built by load_world_data / BoxHull).
struct TraceHull
{
    std::span<const ClipNode32> clipnodes{};
    std::span<const Plane>      planes{};
    int                         firstclipnode = 0;
    int                         lastclipnode  = 0;
    ::xash::utilities::Vec3     clip_mins{}, clip_maxs{};
};

struct TracePlane
{
    ::xash::utilities::Vec3 normal{};
    float                   dist = 0.0f;
};

// pmtrace_t minus ent/hitgroup/deltavelocity — those belong to the physent
// layer (Chunk 6).  Field semantics match the kernel exactly; note the
// kernel writes `endpos` in the HULL-LOCAL frame and finalize_trace
// recomputes the world-frame value from `fraction`.
struct TraceResult
{
    bool  allsolid   = true;  // stays true iff every leaf on the ray was solid
    bool  startsolid = false; // some solid leaf was entered
    bool  inopen     = false;
    bool  inwater    = false;
    float fraction   = 1.0f;  // 1.0 = no impact
    ::xash::utilities::Vec3 endpos{};
    TracePlane plane{};       // valid only when fraction < 1
};

// PM_HullPointContents: returns a CONTENTS_* value (k_contents_none for a
// hull with no planes — legacy "fantom bmodels" guard).
[[nodiscard]] int hull_point_contents( const TraceHull &hull, int num,
                                       const ::xash::utilities::Vec3 &p ) noexcept;

// PM_RecursiveHullCheck — the kernel.  p1f/p2f are fractions of p1/p2 along
// the original ray; p1/p2 are HULL-LOCAL points.  Returns true when the
// subsegment is fully open (legacy qboolean); callers read `trace`.
// Pre: trace initialised (trace_hull() does this); a bad node number logs
// an error and returns false where legacy calls Host_Error.
[[nodiscard]] bool recursive_hull_check( const TraceHull &hull, int num,
                                         float p1f, float p2f,
                                         const ::xash::utilities::Vec3 &p1,
                                         const ::xash::utilities::Vec3 &p2,
                                         TraceResult &trace ) noexcept;

// PM_InitPMTrace + kernel invocation from hull.firstclipnode over [0,1].
// endpos is initialised to `end_local` and stays there unless an impact is
// recorded (legacy memset + VectorCopy(end)).
[[nodiscard]] TraceResult trace_hull( const TraceHull &hull,
                                      const ::xash::utilities::Vec3 &start_local,
                                      const ::xash::utilities::Vec3 &end_local ) noexcept;

// PM_PlayerTraceExt per-entity post-processing (:504-522), extracted
// edict-free for the non-rotated case: allsolid→startsolid,
// startsolid→fraction 0, and for clean traces the world-frame endpos lerp
// plus the plane.dist = dot(endpos, normal) recompute.  (The rotated-entity
// matrix transform arrives with the server chunk.)
void finalize_trace( TraceResult &tr,
                     const ::xash::utilities::Vec3 &start_world,
                     const ::xash::utilities::Vec3 &end_world ) noexcept;

// View over one of a submodel's wired hulls (0-3).  Absent hulls yield
// empty spans (see TraceHull).
[[nodiscard]] TraceHull world_hull( const WorldData &w, std::size_t submodel,
                                    int bsp_hull ) noexcept;

// PM_HullForBsp: usehull → BSP hull remap (1→3 head, 2→0 point, 3→2 large,
// default→1 human) + the centering offset:
// offset = hull.clip_mins - player_bounds.mins + model_origin.
// The caller moves the ray into the local frame: start_l = start - offset.
struct HullSelection
{
    TraceHull               hull;
    ::xash::utilities::Vec3 offset;
};
[[nodiscard]] HullSelection hull_for_bsp( const WorldData &w, std::size_t submodel,
                                          int usehull,
                                          const HullBounds &player_bounds,
                                          const ::xash::utilities::Vec3 &model_origin ) noexcept;

// PM_InitBoxHull/PM_HullForBox as a value type (replaces the legacy shared
// static): six axial planes + the fixed six-node clipnode chain.  For
// entity boxes the caller passes the Minkowski-expanded bounds
// (mins - player_maxs, maxs - player_mins — pm_trace.c:406-409).
class BoxHull
{
public:
    BoxHull() noexcept;

    // Self-referential: hull_ holds spans over the member arrays, so
    // compiler-generated copies/moves would alias the SOURCE object (QJ
    // RAII/self-referential rule).  One BoxHull per callsite.
    BoxHull( const BoxHull & )            = delete;
    BoxHull &operator=( const BoxHull & ) = delete;
    BoxHull( BoxHull && )                 = delete;
    BoxHull &operator=( BoxHull && )      = delete;

    // Updates the six plane distances and returns the hull view.
    // [[nodiscard]] deliberately omitted: mutating the bounds and later
    // reading hull() is a legitimate call pattern.
    const TraceHull &set_bounds( const ::xash::utilities::Vec3 &mins,
                                 const ::xash::utilities::Vec3 &maxs ) noexcept;

    [[nodiscard]] const TraceHull &hull() const noexcept { return hull_; }

private:
    std::array<ClipNode32, 6> clipnodes_;
    std::array<Plane, 6>      planes_;
    TraceHull                 hull_;
};

} // namespace xash::map_loader
