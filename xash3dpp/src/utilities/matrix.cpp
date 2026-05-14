// xash3dpp — matrix implementation
// Legacy reference: public/matrixlib.c
//
// This file only implements the value-type wrappers defined in matrix.hpp.
// Bone/attachment math that depends on com_model.h belongs elsewhere.

#include <xash3dpp/utilities/matrix.hpp>
#include <cmath>
#include <cstring>
#include <numbers>

namespace xash::utilities {

// Degrees-to-radians conversion factor shared by all angle helpers in this file.
inline constexpr float k_deg2rad = static_cast<float>( std::numbers::pi / 180.0 );

// ---------------------------------------------------------------------------
// Matrix3x4
// ---------------------------------------------------------------------------

Matrix3x4 Matrix3x4::identity() noexcept
{
    Matrix3x4 r{};
    r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0f;
    return r;
}

Vec3 transform_point( const Matrix3x4 &m, const Vec3 &v ) noexcept
{
    return {
        v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2] + m.m[0][3],
        v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2] + m.m[1][3],
        v.x * m.m[2][0] + v.y * m.m[2][1] + v.z * m.m[2][2] + m.m[2][3],
    };
}

Vec3 rotate_vector( const Matrix3x4 &m, const Vec3 &v ) noexcept
{
    return {
        v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2],
        v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2],
        v.x * m.m[2][0] + v.y * m.m[2][1] + v.z * m.m[2][2],
    };
}

Matrix3x4 concat( const Matrix3x4 &a, const Matrix3x4 &b ) noexcept
{
    // Treat implicit 4th row of each as [0,0,0,1].
    Matrix3x4 r{};
    r.m[0][0] = a.m[0][0]*b.m[0][0] + a.m[0][1]*b.m[1][0] + a.m[0][2]*b.m[2][0];
    r.m[0][1] = a.m[0][0]*b.m[0][1] + a.m[0][1]*b.m[1][1] + a.m[0][2]*b.m[2][1];
    r.m[0][2] = a.m[0][0]*b.m[0][2] + a.m[0][1]*b.m[1][2] + a.m[0][2]*b.m[2][2];
    r.m[0][3] = a.m[0][0]*b.m[0][3] + a.m[0][1]*b.m[1][3] + a.m[0][2]*b.m[2][3] + a.m[0][3];
    r.m[1][0] = a.m[1][0]*b.m[0][0] + a.m[1][1]*b.m[1][0] + a.m[1][2]*b.m[2][0];
    r.m[1][1] = a.m[1][0]*b.m[0][1] + a.m[1][1]*b.m[1][1] + a.m[1][2]*b.m[2][1];
    r.m[1][2] = a.m[1][0]*b.m[0][2] + a.m[1][1]*b.m[1][2] + a.m[1][2]*b.m[2][2];
    r.m[1][3] = a.m[1][0]*b.m[0][3] + a.m[1][1]*b.m[1][3] + a.m[1][2]*b.m[2][3] + a.m[1][3];
    r.m[2][0] = a.m[2][0]*b.m[0][0] + a.m[2][1]*b.m[1][0] + a.m[2][2]*b.m[2][0];
    r.m[2][1] = a.m[2][0]*b.m[0][1] + a.m[2][1]*b.m[1][1] + a.m[2][2]*b.m[2][1];
    r.m[2][2] = a.m[2][0]*b.m[0][2] + a.m[2][1]*b.m[1][2] + a.m[2][2]*b.m[2][2];
    r.m[2][3] = a.m[2][0]*b.m[0][3] + a.m[2][1]*b.m[1][3] + a.m[2][2]*b.m[2][3] + a.m[2][3];
    return r;
}

Matrix3x4 invert_ortho( const Matrix3x4 &m ) noexcept
{
    // Transpose the 3×3 rotation block; then back-transform the translation.
    // For an orthonormal matrix: R^-1 = R^T, t^-1 = -R^T * t.
    Matrix3x4 r{};
    r.m[0][0] = m.m[0][0];  r.m[0][1] = m.m[1][0];  r.m[0][2] = m.m[2][0];
    r.m[1][0] = m.m[0][1];  r.m[1][1] = m.m[1][1];  r.m[1][2] = m.m[2][1];
    r.m[2][0] = m.m[0][2];  r.m[2][1] = m.m[1][2];  r.m[2][2] = m.m[2][2];
    r.m[0][3] = -( m.m[0][3]*r.m[0][0] + m.m[1][3]*r.m[0][1] + m.m[2][3]*r.m[0][2] );
    r.m[1][3] = -( m.m[0][3]*r.m[1][0] + m.m[1][3]*r.m[1][1] + m.m[2][3]*r.m[1][2] );
    r.m[2][3] = -( m.m[0][3]*r.m[2][0] + m.m[1][3]*r.m[2][1] + m.m[2][3]*r.m[2][2] );
    return r;
}

Matrix3x4 from_angles( const Vec3 &origin, const Vec3 &angles ) noexcept
{
    // Direct port of Matrix3x4_CreateFromEntity (scale = 1).
    // angles: x=pitch, y=yaw, z=roll (degrees)
    float sr, sp, sy, cr, cp, cy;
    Matrix3x4 r{};

    if( angles.z != 0.0f )
    {
        sy = std::sin( angles.y * k_deg2rad );  cy = std::cos( angles.y * k_deg2rad );
        sp = std::sin( angles.x * k_deg2rad );  cp = std::cos( angles.x * k_deg2rad );
        sr = std::sin( angles.z * k_deg2rad );  cr = std::cos( angles.z * k_deg2rad );
        r.m[0][0] = cp*cy;              r.m[0][1] = sr*sp*cy + cr*-sy;  r.m[0][2] = cr*sp*cy + -sr*-sy;  r.m[0][3] = origin.x;
        r.m[1][0] = cp*sy;              r.m[1][1] = sr*sp*sy + cr*cy;   r.m[1][2] = cr*sp*sy + -sr*cy;   r.m[1][3] = origin.y;
        r.m[2][0] = -sp;                r.m[2][1] = sr*cp;               r.m[2][2] = cr*cp;                r.m[2][3] = origin.z;
    }
    else if( angles.x != 0.0f )
    {
        sy = std::sin( angles.y * k_deg2rad );  cy = std::cos( angles.y * k_deg2rad );
        sp = std::sin( angles.x * k_deg2rad );  cp = std::cos( angles.x * k_deg2rad );
        r.m[0][0] = cp*cy;  r.m[0][1] = -sy;  r.m[0][2] = sp*cy;  r.m[0][3] = origin.x;
        r.m[1][0] = cp*sy;  r.m[1][1] =  cy;  r.m[1][2] = sp*sy;  r.m[1][3] = origin.y;
        r.m[2][0] = -sp;    r.m[2][1] = 0.0f; r.m[2][2] = cp;     r.m[2][3] = origin.z;
    }
    else if( angles.y != 0.0f )
    {
        sy = std::sin( angles.y * k_deg2rad );  cy = std::cos( angles.y * k_deg2rad );
        r.m[0][0] = cy;   r.m[0][1] = -sy;  r.m[0][2] = 0.0f;  r.m[0][3] = origin.x;
        r.m[1][0] = sy;   r.m[1][1] =  cy;  r.m[1][2] = 0.0f;  r.m[1][3] = origin.y;
        r.m[2][0] = 0.0f; r.m[2][1] = 0.0f; r.m[2][2] = 1.0f;  r.m[2][3] = origin.z;
    }
    else
    {
        r.m[0][0] = 1.0f;  r.m[0][3] = origin.x;
        r.m[1][1] = 1.0f;  r.m[1][3] = origin.y;
        r.m[2][2] = 1.0f;  r.m[2][3] = origin.z;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Matrix4x4
// ---------------------------------------------------------------------------

Matrix4x4 Matrix4x4::identity() noexcept
{
    Matrix4x4 r{};
    r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
    return r;
}

Matrix4x4 concat( const Matrix4x4 &a, const Matrix4x4 &b ) noexcept
{
    Matrix4x4 r{};
    for( int i = 0; i < 4; ++i )
        for( int j = 0; j < 4; ++j )
            for( int k = 0; k < 4; ++k )
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

Vec3 transform_coord( const Matrix4x4 &m, const Vec3 &v ) noexcept
{
    const float x = v.x*m.m[0][0] + v.y*m.m[0][1] + v.z*m.m[0][2] + m.m[0][3];
    const float y = v.x*m.m[1][0] + v.y*m.m[1][1] + v.z*m.m[1][2] + m.m[1][3];
    const float z = v.x*m.m[2][0] + v.y*m.m[2][1] + v.z*m.m[2][2] + m.m[2][3];
    const float w = v.x*m.m[3][0] + v.y*m.m[3][1] + v.z*m.m[3][2] + m.m[3][3];
    const float iw = ( w != 0.0f ) ? 1.0f / w : 1.0f;
    return { x * iw, y * iw, z * iw };
}

Matrix4x4 perspective( float fov_y, float aspect, float z_near, float z_far ) noexcept
{
    // OpenGL right-handed perspective, NDC z in [-1, 1].
    // fov_y is in degrees.
    const float f = 1.0f / std::tan( fov_y * 0.5f * k_deg2rad );
    Matrix4x4 r{};
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = ( z_far + z_near ) / ( z_near - z_far );
    r.m[2][3] = 2.0f * z_far * z_near / ( z_near - z_far );
    r.m[3][2] = -1.0f;
    return r;
}

Matrix4x4 look_at( const Vec3 &eye, const Vec3 &at, const Vec3 &up ) noexcept
{
    const Vec3 f = normalize( at  - eye );
    const Vec3 r = normalize( cross( f, up ) );
    const Vec3 u = cross( r, f );
    Matrix4x4 m{};
    m.m[0][0] =  r.x;  m.m[0][1] =  r.y;  m.m[0][2] =  r.z;  m.m[0][3] = -dot( r, eye );
    m.m[1][0] =  u.x;  m.m[1][1] =  u.y;  m.m[1][2] =  u.z;  m.m[1][3] = -dot( u, eye );
    m.m[2][0] = -f.x;  m.m[2][1] = -f.y;  m.m[2][2] = -f.z;  m.m[2][3] =  dot( f, eye );
    m.m[3][3] = 1.0f;
    return m;
}

Matrix3x4 to_matrix3x4( const Matrix4x4 &m ) noexcept
{
    Matrix3x4 r{};
    for( int i = 0; i < 3; ++i )
        for( int j = 0; j < 4; ++j )
            r.m[i][j] = m.m[i][j];
    return r;
}

// ---------------------------------------------------------------------------
// Angle helpers
// ---------------------------------------------------------------------------

AngleVectors angle_vectors( const Vec3 &angles ) noexcept
{
    const float sy = std::sin( angles.y * k_deg2rad ),  cy = std::cos( angles.y * k_deg2rad );
    const float sp = std::sin( angles.x * k_deg2rad ),  cp = std::cos( angles.x * k_deg2rad );
    const float sr = std::sin( angles.z * k_deg2rad ),  cr = std::cos( angles.z * k_deg2rad );

    return {
        Vec3{  cp*cy,            cp*sy,           -sp    },   // fwd
        Vec3{ -sr*sp*cy-cr*-sy, -sr*sp*sy-cr*cy,  -sr*cp },   // right
        Vec3{  cr*sp*cy-sr*-sy,  cr*sp*sy-sr*cy,   cr*cp },   // up
    };
}

float vec_to_yaw( const Vec3 &v ) noexcept
{
    if( v.x == 0.0f && v.y == 0.0f ) return 0.0f;
    float yaw = std::atan2( v.y, v.x ) * ( 1.0f / k_deg2rad );
    if( yaw < 0.0f ) yaw += 360.0f;
    return yaw;
}

Vec3 vector_angles( const Vec3 &fwd ) noexcept
{
    float pitch, yaw;
    if( fwd.x == 0.0f && fwd.y == 0.0f )
    {
        yaw   = 0.0f;
        pitch = ( fwd.z > 0.0f ) ? 90.0f : 270.0f;
    }
    else
    {
        yaw = std::atan2( fwd.y, fwd.x ) * ( 1.0f / k_deg2rad );
        if( yaw < 0.0f ) yaw += 360.0f;
        const float xy = std::sqrt( fwd.x * fwd.x + fwd.y * fwd.y );
        pitch = std::atan2( fwd.z, xy ) * ( 1.0f / k_deg2rad );
        if( pitch < 0.0f ) pitch += 360.0f;
    }
    return { pitch, yaw, 0.0f };
}

} // namespace xash::utilities
