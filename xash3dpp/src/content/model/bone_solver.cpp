// xash3dpp — studio bone kernel implementation
// Legacy reference: public/xash3d_mathlib.c (R_StudioCalcBones),
//   engine/common/mod_studio.c (Mod_StudioCalcBoneAdj).
//
// Bit-exact transcription. The RLE span walk, the exact-float VectorCompare gate
// (lerp vs copy), and the AngleQuaternion/QuaternionSlerp composition mirror the
// legacy control flow line-for-line; the trig itself is the golden-verified
// utilities primitives. Hardening additions (guarding a zero/out-of-range RLE
// span, controller-byte bounds) never fire on well-formed data, so parity holds.

#include <xash3dpp/content/bone_solver.hpp>

#include <xash3dpp/limits.hpp>
#include <xash3dpp/utilities/quaternion.hpp>

#include <array>
#include <cmath>

namespace xash::content {

namespace {

// legacy M_PI_F (float((double)M_PI)) — the bone-controller rotation adj factor
// uses float M_PI_F / 180.0f, distinct from the double DEG2RAD elsewhere.
inline constexpr float k_pi_f = static_cast<float>( 3.14159265358979323846 );

// bound( lo, x, hi )
[[nodiscard]] float clampf( float x, float lo, float hi ) noexcept
{
    return ( x >= lo ) ? ( ( x < hi ) ? x : hi ) : lo;
}

} // namespace

void calc_bone_adj( std::span<float> adj, std::span<const std::uint8_t> pcontroller,
                    const StudioView &hdr ) noexcept
{
    const std::int32_t n = hdr.num_bonecontrollers();

    for( std::int32_t j = 0; j < n; ++j )
    {
        const BoneControllerView bc = hdr.bonecontroller( j );
        const std::int32_t i = bc.index();

        if( i == k_studio_mouth )
            continue; // ignore mouth
        if( i >= static_cast<std::int32_t>( ::xash::limits::studio_max_controllers ) )
            continue;
        if( i < 0 || static_cast<std::size_t>( i ) >= pcontroller.size() )
            continue; // hardening: the controller byte must exist
        if( static_cast<std::size_t>( j ) >= adj.size() )
            continue; // hardening: adj slot must exist

        const float ctl = static_cast<float>( pcontroller[static_cast<std::size_t>( i )] );
        float value;

        if( bc.type() & k_studio_rloop )
        {
            value = ctl * ( 360.0f / 256.0f ) + bc.start();
        }
        else
        {
            value = ctl / 255.0f;
            value = clampf( value, 0.0f, 1.0f );
            value = ( 1.0f - value ) * bc.start() + value * bc.end();
        }

        switch( bc.type() & k_studio_types )
        {
        case k_studio_xr:
        case k_studio_yr:
        case k_studio_zr:
            adj[static_cast<std::size_t>( j )] = value * ( k_pi_f / 180.0f );
            break;
        case k_studio_x:
        case k_studio_y:
        case k_studio_z:
            adj[static_cast<std::size_t>( j )] = value;
            break;
        default:
            break;
        }
    }
}

void calc_bones( int frame, float s, const BoneView &bone, const AnimView &anim,
                 std::span<const float> adj,
                 ::xash::utilities::Vec3 &pos, ::xash::utilities::Vec4 *q ) noexcept
{
    float v1[6] = {};
    float v2[6] = {};
    const int max = ( q != nullptr ) ? 6 : 3;

    for( int i = 0; i < max; ++i )
    {
        int   j    = frame;
        float fadj = 0.0f;

        const std::int32_t ctrl = bone.bonecontroller( i );
        if( ctrl >= 0 && static_cast<std::size_t>( ctrl ) < adj.size() )
            fadj = adj[static_cast<std::size_t>( ctrl )];

        if( !anim.has_channel( i ) )
        {
            v1[i] = v2[i] = bone.value( i ) + fadj;
            continue;
        }

        AnimValueCursor cur = anim.channel( i );

        if( cur.total() < cur.valid() )
            j = 0;

        // Walk the RLE spans to the one containing frame j. Legacy assumes
        // total >= 1; a zero (or out-of-range -> 0) total would spin, so break.
        while( cur.total() <= j )
        {
            const int total = cur.total();
            if( total == 0 )
                break;
            j -= total;
            cur = cur.advance( static_cast<std::size_t>( cur.valid() ) + 1 );
            if( cur.total() < cur.valid() )
                j = 0;
        }

        if( cur.valid() > j )
        {
            v1[i] = cur.value( static_cast<std::size_t>( j ) + 1 );

            if( cur.valid() > j + 1 )
                v2[i] = cur.value( static_cast<std::size_t>( j ) + 2 );
            else if( cur.total() > j + 1 )
                v2[i] = v1[i];
            else
                v2[i] = cur.value( static_cast<std::size_t>( cur.valid() ) + 2 );
        }
        else
        {
            v1[i] = cur.value( cur.valid() );

            if( cur.total() > j + 1 )
                v2[i] = v1[i];
            else
                v2[i] = cur.value( static_cast<std::size_t>( cur.valid() ) + 2 );
        }

        v1[i] = bone.value( i ) + v1[i] * bone.scale( i ) + fadj;
        v2[i] = bone.value( i ) + v2[i] * bone.scale( i ) + fadj;
    }

    // Position (channels 0..2): lerp unless exactly equal (VectorCompare uses ==).
    if( v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2] )
    {
        pos.x = v1[0] + s * ( v2[0] - v1[0] );
        pos.y = v1[1] + s * ( v2[1] - v1[1] );
        pos.z = v1[2] + s * ( v2[2] - v1[2] );
    }
    else
    {
        pos = { v1[0], v1[1], v1[2] };
    }

    if( q != nullptr )
    {
        if( v1[3] != v2[3] || v1[4] != v2[4] || v1[5] != v2[5] )
        {
            const ::xash::utilities::Vec4 q1 = ::xash::utilities::angle_quaternion_studio( { v1[3], v1[4], v1[5] } );
            const ::xash::utilities::Vec4 q2 = ::xash::utilities::angle_quaternion_studio( { v2[3], v2[4], v2[5] } );
            *q = ::xash::utilities::quaternion_slerp( q1, q2, s );
        }
        else
        {
            *q = ::xash::utilities::angle_quaternion_studio( { v1[3], v1[4], v1[5] } );
        }
    }
}

namespace {

using ::xash::utilities::Vec3;
using ::xash::utilities::Vec4;

// Mod_StudioCalcRotations — decode every used bone of one blend into pos/q
// (indexed by bone), then remove the driven linear-motion axes of the motion
// bone. pos/q are sized to the header bone count; `panim_base` is the byte
// offset of this blend's mstudioanim_t array.
void calc_rotations( std::span<const int> boneused, std::span<const std::uint8_t> pcontroller,
                     std::span<Vec3> pos, std::span<Vec4> q,
                     const StudioView &hdr, const SeqDescView &seq,
                     std::size_t panim_base, float f ) noexcept
{
    const int numframes = seq.numframes();
    if( f > static_cast<float>( numframes - 1 ) )
        f = 0.0f;
    else if( f < -0.01f )
        f = -0.01f;

    const int   frame = static_cast<int>( f );
    const float s     = f - static_cast<float>( frame );

    float adj[::xash::limits::studio_max_controllers] = {};
    calc_bone_adj( adj, pcontroller, hdr );
    const std::span<const float> adj_span{ adj };

    for( std::size_t jj = boneused.size(); jj-- > 0; ) // j = numbones-1 .. 0
    {
        const std::size_t i = static_cast<std::size_t>( boneused[jj] );
        const BoneView bone = hdr.bone( boneused[jj] );
        const AnimView anim{ hdr.data(), panim_base + i * k_studio_anim_stride };
        calc_bones( frame, s, bone, anim, adj_span, pos[i], &q[i] );
    }

    // linear-motion removal on the motion bone (Mod_StudioCalcRotations tail).
    const int mtype = seq.motiontype();
    const int mbone = seq.motionbone();
    if( mbone >= 0 && static_cast<std::size_t>( mbone ) < pos.size() )
    {
        if( mtype & k_studio_x ) pos[static_cast<std::size_t>( mbone )].x = 0.0f;
        if( mtype & k_studio_y ) pos[static_cast<std::size_t>( mbone )].y = 0.0f;
        if( mtype & k_studio_z ) pos[static_cast<std::size_t>( mbone )].z = 0.0f;
    }
}

} // namespace

int setup_bones( const StudioView &hdr, const BoneSetupInput &in,
                 std::span<::xash::utilities::Matrix3x4> out_bones ) noexcept
{
    const int numbones_hdr = hdr.num_bones();
    if( numbones_hdr <= 0
        || static_cast<std::size_t>( numbones_hdr ) > ::xash::limits::studio_max_bones
        || out_bones.size() < static_cast<std::size_t>( numbones_hdr ) )
        return 0;

    int sequence = in.sequence;
    if( sequence < 0 || sequence >= hdr.num_seq() )
        sequence = 0;

    const SeqDescView seq = hdr.seqdesc( sequence );

    // R_StudioGetAnim (embedded only). External seqgroup -> OOB base -> bind pose.
    std::size_t panim_base;
    if( seq.seqgroup() == 0 && seq.animindex() >= 0 )
        panim_base = static_cast<std::size_t>( seq.animindex() );
    else
        panim_base = hdr.data().size();

    int iBone = in.bone;
    if( iBone < -1 || iBone >= numbones_hdr )
        iBone = 0;

    std::array<int, ::xash::limits::studio_max_bones> boneused{};
    int numbones = 0;
    if( iBone == -1 )
    {
        numbones = numbones_hdr;
        for( int i = 0; i < numbones_hdr; ++i )
            boneused[static_cast<std::size_t>( ( numbones - i ) - 1 )] = i; // reversed fill
    }
    else
    {
        for( int i = iBone; i != -1; i = hdr.bone( i ).parent() )
        {
            if( numbones >= numbones_hdr )
                break; // hardening: malformed parent cycle
            boneused[static_cast<std::size_t>( numbones++ )] = i;
        }
    }

    float f = 0.0f;
    if( seq.numframes() > 1 )
        f = ( in.frame * static_cast<float>( seq.numframes() - 1 ) ) / 256.0f;

    std::array<Vec3, ::xash::limits::studio_max_bones> pos{};
    std::array<Vec4, ::xash::limits::studio_max_bones> q{};
    const std::span<const int> used{ boneused.data(), static_cast<std::size_t>( numbones ) };
    calc_rotations( used, in.controllers, pos, q, hdr, seq, panim_base, f );

    if( seq.numblends() > 1 )
    {
        const auto blend = [&]( std::size_t k ) -> float {
            return k < in.blending.size() ? static_cast<float>( in.blending[k] ) / 255.0f : 0.0f;
        };
        // R_StudioSlerpBones runs over ALL header bones (not the local subset).
        const std::size_t nbh    = static_cast<std::size_t>( numbones_hdr );
        const std::size_t stride = nbh * k_studio_anim_stride;

        std::array<Vec3, ::xash::limits::studio_max_bones> pos2{};
        std::array<Vec4, ::xash::limits::studio_max_bones> q2{};
        panim_base += stride;
        calc_rotations( used, in.controllers, pos2, q2, hdr, seq, panim_base, f );
        ::xash::utilities::slerp_bones( std::span<Vec4>( q.data(), nbh ), std::span<Vec3>( pos.data(), nbh ),
                                        std::span<const Vec4>( q2.data(), nbh ), std::span<const Vec3>( pos2.data(), nbh ),
                                        blend( 0 ) );

        if( seq.numblends() == 4 )
        {
            std::array<Vec3, ::xash::limits::studio_max_bones> pos3{}, pos4{};
            std::array<Vec4, ::xash::limits::studio_max_bones> q3{}, q4{};
            panim_base += stride;
            calc_rotations( used, in.controllers, pos3, q3, hdr, seq, panim_base, f );
            panim_base += stride;
            calc_rotations( used, in.controllers, pos4, q4, hdr, seq, panim_base, f );

            ::xash::utilities::slerp_bones( std::span<Vec4>( q3.data(), nbh ), std::span<Vec3>( pos3.data(), nbh ),
                                            std::span<const Vec4>( q4.data(), nbh ), std::span<const Vec3>( pos4.data(), nbh ),
                                            blend( 0 ) ); // pblending[0] again (legacy quirk)
            ::xash::utilities::slerp_bones( std::span<Vec4>( q.data(), nbh ), std::span<Vec3>( pos.data(), nbh ),
                                            std::span<const Vec4>( q3.data(), nbh ), std::span<const Vec3>( pos3.data(), nbh ),
                                            blend( 1 ) );
        }
    }

    const ::xash::utilities::Matrix3x4 studio_transform =
        ::xash::utilities::create_from_entity( in.origin, in.angles, 1.0f );

    for( std::size_t jj = static_cast<std::size_t>( numbones ); jj-- > 0; )
    {
        const std::size_t i = static_cast<std::size_t>( boneused[jj] );
        const ::xash::utilities::Matrix3x4 bonematrix =
            ::xash::utilities::from_origin_quat( q[i], pos[i] );
        const int parent = hdr.bone( boneused[jj] ).parent();
        if( parent == -1 )
            out_bones[i] = ::xash::utilities::concat( studio_transform, bonematrix );
        else
            out_bones[i] = ::xash::utilities::concat( out_bones[static_cast<std::size_t>( parent )], bonematrix );
    }

    return numbones;
}

bool bone_world_position( const StudioView &hdr, BoneSetupInput in, int bone,
                          IBoneSolver &solver, ::xash::utilities::Vec3 *out_origin,
                          ::xash::utilities::Vec3 *out_angles ) noexcept
{
    if( bone < 0 || bone >= hdr.num_bones() )
        return false;

    in.bone = bone;
    std::array<::xash::utilities::Matrix3x4, ::xash::limits::studio_max_bones> bones{};
    if( solver.setup_bones( hdr, in, bones ) == 0 )
        return false;

    const ::xash::utilities::Matrix3x4 &m = bones[static_cast<std::size_t>( bone )];
    if( out_origin != nullptr )
        *out_origin = { m.m[0][3], m.m[1][3], m.m[2][3] }; // Matrix3x4_OriginFromMatrix
    if( out_angles != nullptr )
        *out_angles = ::xash::utilities::angles_from_matrix( m );
    return true;
}

bool attachment_world_position( const StudioView &hdr, BoneSetupInput in, int att,
                                IBoneSolver &solver, ::xash::utilities::Vec3 *out_origin,
                                ::xash::utilities::Vec3 *out_angles ) noexcept
{
    const int natt = hdr.num_attachments();
    if( natt <= 0 )
        return false;

    const int idx = ( att < 0 ) ? 0 : ( ( att > natt - 1 ) ? natt - 1 : att ); // bound(0, att, natt-1)
    const AttachmentView a = hdr.attachment( idx );
    const int abone = a.bone();
    if( abone < 0 || abone >= hdr.num_bones() )
        return false;

    in.bone = abone;
    std::array<::xash::utilities::Matrix3x4, ::xash::limits::studio_max_bones> bones{};
    if( solver.setup_bones( hdr, in, bones ) == 0 )
        return false;

    // worldPose = studio_bones[bone] * translate(attachment.org)
    ::xash::utilities::Matrix3x4 localPose = ::xash::utilities::Matrix3x4::identity();
    const ::xash::utilities::Vec3 org = a.org();
    localPose.m[0][3] = org.x;
    localPose.m[1][3] = org.y;
    localPose.m[2][3] = org.z;
    const ::xash::utilities::Matrix3x4 worldPose =
        ::xash::utilities::concat( bones[static_cast<std::size_t>( abone )], localPose );

    if( out_origin != nullptr )
        *out_origin = { worldPose.m[0][3], worldPose.m[1][3], worldPose.m[2][3] };
    if( out_angles != nullptr )
        *out_angles = ::xash::utilities::angles_from_matrix( worldPose );
    return true;
}

namespace {

using ::xash::utilities::Matrix3x4;
using ::xash::utilities::Vec3;

// DotProductFabs (xash3d_mathlib.h:97): sum of |aᵢ·bᵢ| — the absolute value of
// each PRODUCT, not |aᵢ|·bᵢ. Identical for non-negative `size` (every stock
// caller) but the faithful form matters when a size component is negative.
[[nodiscard]] float dot_fabs( const Vec3 &a, const Vec3 &b ) noexcept
{
    return std::fabs( a.x * b.x ) + std::fabs( a.y * b.y ) + std::fabs( a.z * b.z );
}

// Mod_SetStudioHullPlane: a plane whose normal is column `axis` of the bone
// matrix, at `offset` along it, ± the Minkowski expansion by `size` (odd faces
// subtract, even faces add — the legacy planenum & 1 test).
[[nodiscard]] StudioHullPlane make_hull_plane( const Matrix3x4 &bone, int axis, float offset,
                                               const Vec3 &size, bool odd ) noexcept
{
    StudioHullPlane pl;
    pl.normal = { bone.m[0][axis], bone.m[1][axis], bone.m[2][axis] };
    pl.dist   = pl.normal.x * bone.m[0][3] + pl.normal.y * bone.m[1][3]
              + pl.normal.z * bone.m[2][3] + offset;
    if( odd )
        pl.dist -= dot_fabs( pl.normal, size );
    else
        pl.dist += dot_fabs( pl.normal, size );
    return pl;
}

} // namespace

int studio_hitbox_hulls( const StudioView &hdr, const BoneSetupInput &in,
                         const ::xash::utilities::Vec3 &size, IBoneSolver &solver,
                         std::span<StudioHitboxHull> out ) noexcept
{
    const int numbones    = hdr.num_bones();
    const int numhitboxes = hdr.num_hitboxes();
    if( numbones <= 0 || numhitboxes <= 0
        || out.size() < static_cast<std::size_t>( numhitboxes ) )
        return 0;

    std::array<Matrix3x4, ::xash::limits::studio_max_bones> bones{};
    BoneSetupInput pose = in;
    pose.bone = -1; // all bones
    if( solver.setup_bones( hdr, pose, bones ) == 0 )
        return 0;

    for( int i = 0; i < numhitboxes; ++i )
    {
        const HitboxView hb = hdr.hitbox( i );
        int bone = hb.bone();
        if( bone < 0 || bone >= numbones )
            bone = 0; // hardening: clamp a bad bone reference
        const Matrix3x4 &m = bones[static_cast<std::size_t>( bone )];
        const Vec3 mn = hb.bbmin();
        const Vec3 mx = hb.bbmax();

        StudioHitboxHull &h = out[static_cast<std::size_t>( i )];
        h.hitgroup  = hb.group();
        h.planes[0] = make_hull_plane( m, 0, mx.x, size, false );
        h.planes[1] = make_hull_plane( m, 0, mn.x, size, true );
        h.planes[2] = make_hull_plane( m, 1, mx.y, size, false );
        h.planes[3] = make_hull_plane( m, 1, mn.y, size, true );
        h.planes[4] = make_hull_plane( m, 2, mx.z, size, false );
        h.planes[5] = make_hull_plane( m, 2, mn.z, size, true );
    }
    return numhitboxes;
}

} // namespace xash::content
