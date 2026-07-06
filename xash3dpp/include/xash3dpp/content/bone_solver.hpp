#pragma once
// xash3dpp — studio bone kernel (RLE animation decode + controller adjustments)
// Legacy reference: public/xash3d_mathlib.c (R_StudioCalcBones — the merged
//   position+rotation decompressor), engine/common/mod_studio.c
//   (Mod_StudioCalcBoneAdj).
//
// These walk the studiohdr byte image (via the studio.hpp sub-views) and drive
// the pure math primitives in utilities/quaternion.hpp. They are the struct-
// aware layer of the bone solver; the pure trig/matrix kernels live in
// utilities (boundary OQ-5). The setup-bones driver (Phase C) composes these
// per bone.
//
// @thread-safety: pure functions over caller-owned data — no shared state.

#include <xash3dpp/content/studio.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <cstdint>
#include <span>

namespace xash::content {

// Mod_StudioCalcBoneAdj — fill `adj` (indexed by bone-controller slot) from the
// entity's raw controller bytes. The caller zero-initialises `adj` (unmatched
// slots stay 0); `adj` must be at least num_bonecontrollers long. `pcontroller`
// supplies one byte per controller index the model references (mouth included);
// a referenced byte past its end is skipped (hardening — legacy indexes it raw).
void calc_bone_adj( std::span<float> adj, std::span<const std::uint8_t> pcontroller,
                    const StudioView &hdr ) noexcept;

// R_StudioCalcBones — decode bone `bone` at (frame, s) from anim `anim`,
// applying controller adjustments `adj`. Writes `pos` always; writes `*q` only
// when q != nullptr (position-only otherwise, matching the legacy max = q?6:3).
// `adj` may be empty (no controllers). Bit-exact to the legacy decoder: the RLE
// span walk, the exact-float lerp/copy gate, and the AngleQuaternion/Slerp
// composition are reproduced verbatim.
void calc_bones( int frame, float s, const BoneView &bone, const AnimView &anim,
                 std::span<const float> adj,
                 ::xash::utilities::Vec3 &pos, ::xash::utilities::Vec4 *q ) noexcept;

} // namespace xash::content
