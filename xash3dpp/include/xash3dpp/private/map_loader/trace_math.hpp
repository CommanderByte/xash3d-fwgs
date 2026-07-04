#pragma once
// xash3dpp — plane math shared by the PVS walkers and the trace kernel.
// Legacy reference: public/xash3d_mathlib.h — PlaneDiff (:140),
// DotProduct (:96), BOX_ON_PLANE_SIDE (:194-208); com_model.h:572.
//
// PARITY-CRITICAL (Q-18): plane_diff must keep the legacy axial fast path —
// for plane types 0-2 the coordinate is read directly instead of computing
// the dot product.  The two forms are algebraically equal for unit axial
// normals but may differ in the last ULP; trace/PVS tie-breaks sit exactly
// on those boundaries.  The dot product is the plain single-precision
// three-term sum (NOT a higher-precision variant).

#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/utilities/math.hpp>

namespace xash::map_loader {

// (k_dist_epsilon lives in the public trace.hpp.)

[[nodiscard]] inline float vec3_component( const ::xash::utilities::Vec3 &v, int i ) noexcept
{
    return i == 0 ? v.x : i == 1 ? v.y : v.z;
}

// Legacy PlaneDiff: ((type < 3) ? point[type] : DotProduct(point, normal)) - dist
[[nodiscard]] inline float plane_diff( const ::xash::utilities::Vec3 &p,
                                       const Plane &plane ) noexcept
{
    const float d = plane.type < 3
        ? vec3_component( p, plane.type )
        : p.x * plane.normal.x + p.y * plane.normal.y + p.z * plane.normal.z;
    return d - plane.dist;
}

// Legacy BOX_ON_PLANE_SIDE: 1 = box fully in front, 2 = fully behind,
// 3 = straddles.  Axial fast path, then the signbits-indexed corner test.
[[nodiscard]] inline int box_on_plane_side( const ::xash::utilities::Vec3 &emins,
                                            const ::xash::utilities::Vec3 &emaxs,
                                            const Plane &p ) noexcept
{
    if ( p.type < 3 )
    {
        if ( p.dist <= vec3_component( emins, p.type ))
            return 1;
        if ( p.dist >= vec3_component( emaxs, p.type ))
            return 2;
        return 3;
    }

    // BoxOnPlaneSide (xash3d_mathlib.c:382-435): pick the near/far box
    // corners by signbits; term order kept for float-exactness.
    float dist1, dist2;
    switch ( p.signbits )
    {
    case 0:
        dist1 = p.normal.x * emaxs.x + p.normal.y * emaxs.y + p.normal.z * emaxs.z;
        dist2 = p.normal.x * emins.x + p.normal.y * emins.y + p.normal.z * emins.z;
        break;
    case 1:
        dist1 = p.normal.x * emins.x + p.normal.y * emaxs.y + p.normal.z * emaxs.z;
        dist2 = p.normal.x * emaxs.x + p.normal.y * emins.y + p.normal.z * emins.z;
        break;
    case 2:
        dist1 = p.normal.x * emaxs.x + p.normal.y * emins.y + p.normal.z * emaxs.z;
        dist2 = p.normal.x * emins.x + p.normal.y * emaxs.y + p.normal.z * emins.z;
        break;
    case 3:
        dist1 = p.normal.x * emins.x + p.normal.y * emins.y + p.normal.z * emaxs.z;
        dist2 = p.normal.x * emaxs.x + p.normal.y * emaxs.y + p.normal.z * emins.z;
        break;
    case 4:
        dist1 = p.normal.x * emaxs.x + p.normal.y * emaxs.y + p.normal.z * emins.z;
        dist2 = p.normal.x * emins.x + p.normal.y * emins.y + p.normal.z * emaxs.z;
        break;
    case 5:
        dist1 = p.normal.x * emins.x + p.normal.y * emaxs.y + p.normal.z * emins.z;
        dist2 = p.normal.x * emaxs.x + p.normal.y * emins.y + p.normal.z * emaxs.z;
        break;
    case 6:
        dist1 = p.normal.x * emaxs.x + p.normal.y * emins.y + p.normal.z * emins.z;
        dist2 = p.normal.x * emins.x + p.normal.y * emaxs.y + p.normal.z * emaxs.z;
        break;
    case 7:
        dist1 = p.normal.x * emins.x + p.normal.y * emins.y + p.normal.z * emins.z;
        dist2 = p.normal.x * emaxs.x + p.normal.y * emaxs.y + p.normal.z * emaxs.z;
        break;
    default:
        dist1 = dist2 = 0.0f; // legacy "shut up compiler" arm
        break;
    }

    int sides = 0;
    if ( dist1 >= p.dist )
        sides = 1;
    if ( dist2 < p.dist )
        sides |= 2;
    return sides;
}

} // namespace xash::map_loader
