// xash3dpp — studio bone-math primitives implementation
// Legacy reference: public/xash3d_mathlib.c, public/matrixlib.c (see header).
//
// Bit-exact transcription of the legacy kernels. The double casts on the trig
// (std::sin/cos/acos/atan2/sqrt) are deliberate: the legacy math runs in double
// and narrows to float on store; std::sin(float) would pick sinf and diverge.

#include <xash3dpp/utilities/quaternion.hpp>

#include <cmath>
#include <cstddef>

namespace xash::utilities {

namespace {

// Legacy M_PI and its derived factors, in the exact precision the kernels use.
inline constexpr double k_pi_d    = 3.14159265358979323846;      // M_PI
inline constexpr float  k_pi_f    = static_cast<float>( k_pi_d ); // M_PI_F
inline constexpr double k_rad2deg = 180.0 / k_pi_d;              // RAD2DEG factor

// QuaternionAlign: qt = (sum (p-q)^2 > sum (p+q)^2) ? -q : q. Accumulation order
// matches the legacy i=0..3 loop (a and b are independent accumulators).
[[nodiscard]] Vec4 quaternion_align( const Vec4 &p, const Vec4 &q ) noexcept
{
    float a = 0.0f, b = 0.0f;
    a += ( p.x - q.x ) * ( p.x - q.x );  b += ( p.x + q.x ) * ( p.x + q.x );
    a += ( p.y - q.y ) * ( p.y - q.y );  b += ( p.y + q.y ) * ( p.y + q.y );
    a += ( p.z - q.z ) * ( p.z - q.z );  b += ( p.z + q.z ) * ( p.z + q.z );
    a += ( p.w - q.w ) * ( p.w - q.w );  b += ( p.w + q.w ) * ( p.w + q.w );
    if( a > b )
        return { -q.x, -q.y, -q.z, -q.w };
    return q;
}

} // namespace

Vec4 angle_quaternion_studio( const Vec3 &angles ) noexcept
{
    // studio branch: radians-direct half-angles, legacy variable mapping.
    const auto [sy, cy] = sincos( angles.z * 0.5f );  // ROLL
    const auto [sp, cp] = sincos( angles.y * 0.5f );  // YAW
    const auto [sr, cr] = sincos( angles.x * 0.5f );  // PITCH

    return {
        sr * cp * cy - cr * sp * sy,  // X
        cr * sp * cy + sr * cp * sy,  // Y
        cr * cp * sy - sr * sp * cy,  // Z
        cr * cp * cy + sr * sp * sy,  // W
    };
}

Vec4 quaternion_slerp( const Vec4 &p, const Vec4 &q, float t ) noexcept
{
    // QuaternionSlerp = QuaternionAlign then QuaternionSlerpNoAlign.
    const Vec4 q2 = quaternion_align( p, q );

    const float cosom = p.x * q2.x + p.y * q2.y + p.z * q2.z + p.w * q2.w;

    Vec4 qt{};
    if( ( 1.0f + cosom ) > 0.000001f )
    {
        float sclp, sclq;
        if( ( 1.0f - cosom ) > 0.000001f )
        {
            const float omega = static_cast<float>( std::acos( static_cast<double>( cosom ) ) );
            const float sinom = static_cast<float>( std::sin( static_cast<double>( omega ) ) );
            sclp = static_cast<float>( std::sin( static_cast<double>( ( 1.0f - t ) * omega ) ) / sinom );
            sclq = static_cast<float>( std::sin( static_cast<double>( t * omega ) ) / sinom );
        }
        else
        {
            sclp = 1.0f - t;
            sclq = t;
        }
        qt.x = sclp * p.x + sclq * q2.x;
        qt.y = sclp * p.y + sclq * q2.y;
        qt.z = sclp * p.z + sclq * q2.z;
        qt.w = sclp * p.w + sclq * q2.w;
    }
    else
    {
        // antipodal fallback: build a perpendicular quaternion, then blend only
        // components 0..2 (the legacy loop is i<3 — qt.w keeps q2.z).
        qt.x = -q2.y;
        qt.y = q2.x;
        qt.z = -q2.w;
        qt.w = q2.z;
        const float sclp = static_cast<float>( std::sin( static_cast<double>( ( 1.0f - t ) * ( 0.5f * k_pi_f ) ) ) );
        const float sclq = static_cast<float>( std::sin( static_cast<double>( t * ( 0.5f * k_pi_f ) ) ) );
        qt.x = sclp * p.x + sclq * qt.x;
        qt.y = sclp * p.y + sclq * qt.y;
        qt.z = sclp * p.z + sclq * qt.z;
    }
    return qt;
}

Matrix3x4 from_origin_quat( const Vec4 &q, const Vec3 &origin ) noexcept
{
    // Matrix3x4_FromOriginQuat — map q{x,y,z,w} to the legacy quaternion[0..3].
    Matrix3x4 r{};
    r.m[0][0] = 1.0f - 2.0f * q.y * q.y - 2.0f * q.z * q.z;
    r.m[1][0] = 2.0f * q.x * q.y + 2.0f * q.w * q.z;
    r.m[2][0] = 2.0f * q.x * q.z - 2.0f * q.w * q.y;

    r.m[0][1] = 2.0f * q.x * q.y - 2.0f * q.w * q.z;
    r.m[1][1] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z;
    r.m[2][1] = 2.0f * q.y * q.z + 2.0f * q.w * q.x;

    r.m[0][2] = 2.0f * q.x * q.z + 2.0f * q.w * q.y;
    r.m[1][2] = 2.0f * q.y * q.z - 2.0f * q.w * q.x;
    r.m[2][2] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y;

    r.m[0][3] = origin.x;
    r.m[1][3] = origin.y;
    r.m[2][3] = origin.z;
    return r;
}

Vec3 angles_from_matrix( const Matrix3x4 &m ) noexcept
{
    // Matrix3x4_AnglesFromMatrix. xyDist and every atan2 run in double (legacy
    // sqrt/atan2 promote their float args), the products stay float.
    const float xyDist = static_cast<float>( std::sqrt(
        static_cast<double>( m.m[0][0] * m.m[0][0] + m.m[1][0] * m.m[1][0] ) ) );

    Vec3 out{};
    if( xyDist > 0.001f )
    {
        out.x = static_cast<float>( std::atan2( static_cast<double>( -m.m[2][0] ), static_cast<double>( xyDist ) ) * k_rad2deg );
        out.y = static_cast<float>( std::atan2( static_cast<double>(  m.m[1][0] ), static_cast<double>( m.m[0][0] ) ) * k_rad2deg );
        out.z = static_cast<float>( std::atan2( static_cast<double>(  m.m[2][1] ), static_cast<double>( m.m[2][2] ) ) * k_rad2deg );
    }
    else
    {
        // forward is mostly Z, gimbal lock.
        out.x = static_cast<float>( std::atan2( static_cast<double>( -m.m[2][0] ), static_cast<double>( xyDist ) ) * k_rad2deg );
        out.y = static_cast<float>( std::atan2( static_cast<double>( -m.m[0][1] ), static_cast<double>( m.m[1][1] ) ) * k_rad2deg );
        out.z = 0.0f;
    }
    return out;
}

void slerp_bones( std::span<Vec4> q1, std::span<Vec3> pos1,
                  std::span<const Vec4> q2, std::span<const Vec3> pos2, float s ) noexcept
{
    // s = bound( 0, s, 1 )
    s = ( s >= 0.0f ) ? ( ( s < 1.0f ) ? s : 1.0f ) : 0.0f;

    const std::size_t n = q1.size();
    for( std::size_t i = 0; i < n; ++i )
    {
        q1[i] = quaternion_slerp( q1[i], q2[i], s );
        // VectorLerp: pos1 = pos1 + s * ( pos2 - pos1 )
        pos1[i].x = pos1[i].x + s * ( pos2[i].x - pos1[i].x );
        pos1[i].y = pos1[i].y + s * ( pos2[i].y - pos1[i].y );
        pos1[i].z = pos1[i].z + s * ( pos2[i].z - pos1[i].z );
    }
}

} // namespace xash::utilities
