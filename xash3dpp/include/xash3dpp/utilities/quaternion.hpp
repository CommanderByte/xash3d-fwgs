#pragma once
// xash3dpp — studio bone-math primitives (quaternion + bone blend)
// Legacy reference: public/xash3d_mathlib.c (AngleQuaternion, QuaternionSlerp /
//   QuaternionAlign / QuaternionSlerpNoAlign, R_StudioSlerpBones) and
//   public/matrixlib.c (Matrix3x4_FromOriginQuat, Matrix3x4_AnglesFromMatrix).
//
// These are the pure-float, com_model-free math kernels that the studio bone
// solver (content) composes — promoted to `utilities` per boundary OQ-5. They
// take plain Vec3/Vec4/Matrix3x4 values and know nothing about studiohdr byte
// layout; the struct-walking driver lives in content.
//
// PARITY: every trig path routes through utilities::sincos (the double-precision
// SinCos) and forces double acos/atan2/sqrt with float narrowing on store, so
// the port is bit-exact to the legacy kernels — cross-checked against the Q-18
// verbatim-legacy goldens in tests/utilities/test_quaternion.cpp. The arithmetic
// ordering is load-bearing; do not "simplify" it.
//
// @thread-safety: pure value functions — safe from any thread.

#include <xash3dpp/utilities/math.hpp>
#include <xash3dpp/utilities/matrix.hpp>

#include <span>

namespace xash::utilities {

// AngleQuaternion (studio branch): Euler angles in RADIANS (x=pitch, y=yaw,
// z=roll — the studio bone value[3..5] convention) -> unit quaternion {x,y,z,w}.
// Radians-direct (no deg2rad) with the legacy half-angle ROLL->sy / YAW->sp /
// PITCH->sr variable mapping.
[[nodiscard]] Vec4 angle_quaternion_studio( const Vec3 &angles_rad ) noexcept;

// QuaternionSlerp: align p/q (flip q when the sum-of-squares test says it is
// backwards) then spherical-lerp by t. Reproduces the legacy 3-branch NoAlign:
// acos/sin interior, linear (1-t, t) near-parallel, and the antipodal fallback
// (perpendicular quaternion + the i<3 partial blend). t is NOT clamped here
// (the caller's contract; slerp_bones clamps before calling).
[[nodiscard]] Vec4 quaternion_slerp( const Vec4 &p, const Vec4 &q, float t ) noexcept;

// Matrix3x4_FromOriginQuat: quaternion (need not be unit) + origin -> affine
// transform. Pure algebra, no trig.
[[nodiscard]] Matrix3x4 from_origin_quat( const Vec4 &q, const Vec3 &origin ) noexcept;

// Matrix3x4_AnglesFromMatrix: recover Euler angles (DEGREES) from the rotation
// block, with the xyDist <= 0.001 gimbal-lock branch. Double atan2 + RAD2DEG.
[[nodiscard]] Vec3 angles_from_matrix( const Matrix3x4 &m ) noexcept;

// R_StudioSlerpBones: in-place per-bone blend of (q1,pos1) toward (q2,pos2) by
// s (clamped to [0,1]). Orientations slerp, positions lerp linearly. The four
// spans must share the same length (the bone count); slerps run over q1.size().
void slerp_bones( std::span<Vec4> q1, std::span<Vec3> pos1,
                  std::span<const Vec4> q2, std::span<const Vec3> pos2, float s ) noexcept;

} // namespace xash::utilities
