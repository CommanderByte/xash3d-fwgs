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

} // namespace xash::content
