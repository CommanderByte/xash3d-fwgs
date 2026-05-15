#pragma once
// xash3dpp — matrix operations
// Legacy reference: public/matrixlib.c
//
// The legacy code operated on raw float[3][4] and float[4][4] C arrays.
// Here we wrap them in value types so the compiler can reason about alignment
// and we avoid the raw-pointer aliasing issues in the legacy code.
//
// NOTE: The legacy matrixlib.c depends on common/com_model.h for bone/
// attachment types.  This module does NOT import com_model.h; bone-aware
// helpers belong in the content-loaders or server subsystem.

#include "math.hpp"
#include <array>

namespace xash::utilities {

// ---------------------------------------------------------------------------
// Matrix3x4 — affine transform (rotation + translation, no projection)
// ---------------------------------------------------------------------------

struct Matrix3x4
{
    std::array<std::array<float, 4>, 3> m{};

    [[nodiscard]] static Matrix3x4 identity() noexcept;

    // Raw float pointer — for passing to C APIs (GL, renderer, BSP).
    [[nodiscard]] const float *data() const noexcept { return m[0].data(); }
    [[nodiscard]] float       *data()       noexcept { return m[0].data(); }
};

// Transform a point (applies translation).
[[nodiscard]] Vec3 transform_point( const Matrix3x4 &m, const Vec3 &v ) noexcept;

// Rotate a vector (ignores translation row).
[[nodiscard]] Vec3 rotate_vector( const Matrix3x4 &m, const Vec3 &v ) noexcept;

// Concatenate two transforms:  out = a * b
[[nodiscard]] Matrix3x4 concat( const Matrix3x4 &a, const Matrix3x4 &b ) noexcept;

// Invert an orthonormal transform (rotation-only, no scale).
[[nodiscard]] Matrix3x4 invert_ortho( const Matrix3x4 &m ) noexcept;

// Build from origin + euler angles (deg).
[[nodiscard]] Matrix3x4 from_angles( const Vec3 &origin, const Vec3 &angles ) noexcept;

// ---------------------------------------------------------------------------
// Matrix4x4 — general projective transform
// ---------------------------------------------------------------------------

struct Matrix4x4
{
    std::array<std::array<float, 4>, 4> m{};

    [[nodiscard]] static Matrix4x4 identity() noexcept;

    [[nodiscard]] const float *data() const noexcept { return m[0].data(); }
    [[nodiscard]] float       *data()       noexcept { return m[0].data(); }
};

[[nodiscard]] Matrix4x4 concat( const Matrix4x4 &a, const Matrix4x4 &b ) noexcept;
[[nodiscard]] Vec3       transform_coord( const Matrix4x4 &m, const Vec3 &v ) noexcept;

// Build a perspective projection matrix.
[[nodiscard]] Matrix4x4 perspective( float fov_y, float aspect, float z_near, float z_far ) noexcept;

// Build a look-at view matrix.
[[nodiscard]] Matrix4x4 look_at( const Vec3 &eye, const Vec3 &at, const Vec3 &up ) noexcept;

// Extract a Matrix3x4 from the upper 3×4 of a Matrix4x4.
[[nodiscard]] Matrix3x4 to_matrix3x4( const Matrix4x4 &m ) noexcept;

// ---------------------------------------------------------------------------
// Operator overloads — inline forwarders to the named functions above.
// ---------------------------------------------------------------------------

// Matrix3x4 * Matrix3x4  →  concat (composition)
[[nodiscard]] inline Matrix3x4 operator*( const Matrix3x4 &a, const Matrix3x4 &b ) noexcept { return concat( a, b ); }
// Matrix4x4 * Matrix4x4  →  concat
[[nodiscard]] inline Matrix4x4 operator*( const Matrix4x4 &a, const Matrix4x4 &b ) noexcept { return concat( a, b ); }
// Matrix3x4 * Vec3  →  transform_point (rotation + translation)
[[nodiscard]] inline Vec3      operator*( const Matrix3x4 &m, const Vec3 &v ) noexcept { return transform_point( m, v ); }
// Matrix4x4 * Vec3  →  transform_coord (full projective, w-divide)
[[nodiscard]] inline Vec3      operator*( const Matrix4x4 &m, const Vec3 &v ) noexcept { return transform_coord( m, v ); }

} // namespace xash::utilities
