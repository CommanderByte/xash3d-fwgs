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
#include <xash3dpp/utilities/matrix.hpp>

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

// ---------------------------------------------------------------------------
// SV_StudioSetupBones — the driver + the swappable solver seam (boundary OQ-5)
// ---------------------------------------------------------------------------

// Entity pose + blend state for a setup-bones call.
struct BoneSetupInput
{
    float                          frame    = 0.0f;
    int                            sequence = 0;
    ::xash::utilities::Vec3        angles{};
    ::xash::utilities::Vec3        origin{};
    std::span<const std::uint8_t>  controllers{}; // pcontroller (per controller slot)
    std::span<const std::uint8_t>  blending{};    // pblending (blend weights)
    int                            bone = -1;      // iBone: -1 = all bones, else parent chain
};

// SV_StudioSetupBones — build per-bone world transforms into `out_bones` (indexed
// by bone; must be at least num_bones long) from the studio header + pose.
// Returns the number of bones written, or 0 on a bad/empty header. Embedded
// animations only (seqgroup == 0); an external seqgroup degrades to bind pose
// (OQ-7 residue — external "...NN.mdl" sequence loads are deferred).
[[nodiscard]] int setup_bones( const StudioView &hdr, const BoneSetupInput &in,
                               std::span<::xash::utilities::Matrix3x4> out_bones ) noexcept;

// The swappable bone-solver seam (legacy pBlendAPI->SV_StudioSetupBones). The
// builtin reproduces the engine's gBlendAPI fallback; a game DLL can substitute
// its own solver at the server studio-hull provider (OQ-5 -> server OQ-2).
struct IBoneSolver
{
    virtual ~IBoneSolver() = default;

    [[nodiscard]] virtual int setup_bones( const StudioView &hdr, const BoneSetupInput &in,
                                           std::span<::xash::utilities::Matrix3x4> out_bones ) noexcept = 0;
};

// The builtin bone solver (gBlendAPI equivalent) — delegates to the free
// setup_bones. Stateless: safe to share, and each call owns its scratch.
class BuiltinBoneSolver final : public IBoneSolver
{
public:
    [[nodiscard]] int setup_bones( const StudioView &hdr, const BoneSetupInput &in,
                                   std::span<::xash::utilities::Matrix3x4> out_bones ) noexcept override
    {
        return ::xash::content::setup_bones( hdr, in, out_bones );
    }
};

// ---------------------------------------------------------------------------
// Studio pose queries (Mod_GetBonePosition / Mod_StudioGetAttachment)
// ---------------------------------------------------------------------------

// Mod_GetBonePosition — world origin/angles of bone `bone` under pose `in`
// (in.bone is overridden). Writes out_origin/out_angles when non-null; returns
// false on a bad header or out-of-range bone (the caller keeps its defaults).
// NO pitch flip — that is the caller's (attachment/hull) concern.
[[nodiscard]] bool bone_world_position( const StudioView &hdr, BoneSetupInput in, int bone,
                                        IBoneSolver &solver,
                                        ::xash::utilities::Vec3 *out_origin,
                                        ::xash::utilities::Vec3 *out_angles ) noexcept;

// Mod_StudioGetAttachment — world origin/angles of attachment `att` (clamped to
// [0, numattachments-1]). Sets up the attachment bone's chain, concats the
// attachment's local offset, reads the world pose. `out_angles` is the world
// orientation (the caller applies the legacy ENGINE_COMPUTE_STUDIO_LERP gate).
// Returns false when there are no attachments or the header is bad.
[[nodiscard]] bool attachment_world_position( const StudioView &hdr, BoneSetupInput in, int att,
                                              IBoneSolver &solver,
                                              ::xash::utilities::Vec3 *out_origin,
                                              ::xash::utilities::Vec3 *out_angles ) noexcept;

// ---------------------------------------------------------------------------
// Studio hitbox hulls (Mod_HullForStudio / Mod_SetStudioHullPlane)
// ---------------------------------------------------------------------------

// One face of a studio hitbox hull: a world-space plane (legacy mplane_t with
// type 5, non-axial). normal · x <= dist is the inside half-space.
struct StudioHullPlane
{
    ::xash::utilities::Vec3 normal{};
    float                   dist = 0.0f;
};

// A studio hitbox as an oriented box: six planes (axis columns of the bone
// matrix; even faces use bbmax + the Minkowski expansion, odd faces bbmin -)
// plus the hit group. The server wires these into its trace hull + hitgroup.
struct StudioHitboxHull
{
    StudioHullPlane planes[6];
    int             hitgroup = 0;
};

// Mod_HullForStudio (the plane-production half): setup_bones for the pose, then
// emit six oriented planes per hitbox, Minkowski-expanded by the trace box
// half-extents `size`. Fills out[0..numhitboxes) and returns the hitbox count,
// or 0 (bad/empty header, or `out` too small). NO pitch flip and no CS-shield
// skip — the caller (which has the edict + host features) applies those.
[[nodiscard]] int studio_hitbox_hulls( const StudioView &hdr, const BoneSetupInput &in,
                                       const ::xash::utilities::Vec3 &size, IBoneSolver &solver,
                                       std::span<StudioHitboxHull> out ) noexcept;

} // namespace xash::content
