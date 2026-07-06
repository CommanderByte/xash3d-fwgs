#pragma once
// xash3dpp — vector and scalar math
// Legacy reference: public/xash3d_mathlib.h
//
// Replaces the macro soup with constexpr functions and function templates.
// The underlying scalar type is still float (vec_t) to match the SDK ABI.
//
// @thread-safety: pure constexpr math — safe from any thread.

#include <cmath>
#include <cstring>
#include <algorithm>

namespace xash::utilities {

using vec_t = float;

struct Vec2 { vec_t x, y; };
struct Vec3
{
    vec_t x, y, z;

    // Member convenience — free functions remain canonical.
    [[nodiscard]] [[nodiscard]] constexpr vec_t dot( const Vec3 &o ) const noexcept
    {
        return x * o.x + y * o.y + z * o.z;
    }
    [[nodiscard]] float length() const noexcept
    {
        return std::sqrt( x*x + y*y + z*z );
    }
    [[nodiscard]] Vec3 normalized() const noexcept
    {
        const float l = length();
        return l > 0.0f ? Vec3{ x/l, y/l, z/l } : Vec3{};
    }

    constexpr Vec3 &operator+=( const Vec3 &o ) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3 &operator-=( const Vec3 &o ) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3 &operator*=( vec_t s )        noexcept { x *= s;   y *= s;   z *= s;   return *this; }
};
struct Vec4 { vec_t x, y, z, w; };

// Euler angle component indices — match legacy PITCH/YAW/ROLL values.
static constexpr int PITCH = 0;
static constexpr int YAW   = 1;
static constexpr int ROLL  = 2;

// ---------------------------------------------------------------------------
// Scalar helpers
// ---------------------------------------------------------------------------

[[nodiscard]] constexpr float rint( float x ) noexcept
{
    return x < 0.0f ? static_cast<float>( static_cast<int>( x - 0.5f ) )
                    : static_cast<float>( static_cast<int>( x + 0.5f ) );
}

[[nodiscard]] constexpr bool is_nan( float v ) noexcept { return v != v; }

// Legacy SinCos parity. The engine's SinCos() (xash3d_mathlib.h) computes the
// sine/cosine with the DOUBLE-precision sin()/cos() — the float argument is
// promoted — then narrows the result to float on store. Studio bone math is
// bit-sensitive to this exact rounding, so every studio-math primitive routes
// its trig through here and NEVER through std::sin(float)/std::cos(float)
// (which would pick the single-precision sinf/cosf and diverge in the low bits).
struct SinCos { float s, c; };
[[nodiscard]] inline SinCos sincos( float radians ) noexcept
{
    const double r = static_cast<double>( radians );
    return { static_cast<float>( std::sin( r ) ), static_cast<float>( std::cos( r ) ) };
}

// ---------------------------------------------------------------------------
// Vec3 operations
// ---------------------------------------------------------------------------

[[nodiscard]] constexpr vec_t dot( const Vec3 &a, const Vec3 &b ) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] constexpr Vec3 cross( const Vec3 &a, const Vec3 &b ) noexcept
{
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

[[nodiscard]] constexpr Vec3 operator+( const Vec3 &a, const Vec3 &b ) noexcept { return { a.x+b.x, a.y+b.y, a.z+b.z }; }
[[nodiscard]] constexpr Vec3 operator-( const Vec3 &a, const Vec3 &b ) noexcept { return { a.x-b.x, a.y-b.y, a.z-b.z }; }
[[nodiscard]] constexpr Vec3 operator*( const Vec3 &v, vec_t s ) noexcept { return { v.x*s, v.y*s, v.z*s }; }
[[nodiscard]] constexpr Vec3 operator*( vec_t s, const Vec3 &v ) noexcept { return v * s; }

[[nodiscard]] inline float length( const Vec3 &v ) noexcept { return std::sqrt( dot( v, v ) ); }
[[nodiscard]] inline Vec3  normalize( const Vec3 &v ) noexcept { float l = length( v ); return l > 0.0f ? v * (1.0f/l) : Vec3{}; }

// ---------------------------------------------------------------------------
// Angle / rotation helpers
// ---------------------------------------------------------------------------

// Euler angles (deg) → forward/right/up vectors.
// Legacy: AngleVectors
struct AngleVectors { Vec3 fwd, right, up; };
[[nodiscard]] AngleVectors angle_vectors( const Vec3 &angles ) noexcept;

// forward vector → yaw angle (deg).
// Legacy: VecToYaw / SV_VecToYaw
[[nodiscard]] float vec_to_yaw( const Vec3 &v ) noexcept;

// forward vector → pitch+yaw angles (deg).
// Legacy: VectorAngles
[[nodiscard]] Vec3 vector_angles( const Vec3 &fwd ) noexcept;

} // namespace xash::utilities
