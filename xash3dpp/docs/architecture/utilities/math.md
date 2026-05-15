# Math and matrix utilities

> **Headers**: `xash3dpp/include/xash3dpp/utilities/math.hpp`,
> `xash3dpp/include/xash3dpp/utilities/matrix.hpp`  
> **Sources**: `math.hpp` is header-only; `xash3dpp/src/utilities/matrix.cpp`  
> **Namespace**: `xash::utilities`  
> **Legacy reference**: `public/matrixlib.c`, `common/com_model.h` (raw array types)

## Purpose

Provides typed 2-D/3-D/4-D vector types, a complete set of scalar and vector
math helpers, and two matrix types (`Matrix3x4` for affine transforms, `Matrix4x4`
for projective transforms). These replace the legacy `vec3_t float[3]` raw arrays
and the `matrix3x4` / `matrix4x4` raw 2-D float arrays from `matrixlib.c`.

The entire `math.hpp` is header-only (all functions `inline`). `matrix.hpp`
declares types and free functions; non-trivial implementations live in `matrix.cpp`.

## Vector types

```cpp
using vec_t = float;

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };
```

The structs are standard-layout and aggregate-initializable. They carry named
members (`x`, `y`, `z`, `w`) rather than an indexed array to make intent
explicit at call sites. A `.data()` accessor is available where needed for
interop with C-API functions that expect `float *`.

Constants `PITCH = 0`, `YAW = 1`, `ROLL = 2` name the rotation axis indices,
preserving the legacy Quake convention without requiring magic-number indexing.

## Scalar helpers

```cpp
constexpr float rint  ( float f ) noexcept;   // round to nearest integer
constexpr bool  is_nan( float f ) noexcept;   // IEEE 754 NaN check
```

Both are `constexpr`. `rint` rounds half-values away from zero (same behaviour
as the legacy `rint` macro in `crtlib.h`). `is_nan` uses `f != f` to detect NaN
without including `<cmath>` in headers.

## Vec3 member functions

`Vec3` provides a small set of member convenience functions in addition to the
free-function API below:

```cpp
constexpr vec_t dot ( const Vec3 &o ) const noexcept;
float           length()             const noexcept;
Vec3            normalized()         const noexcept;  // zero-vec if length < epsilon

constexpr Vec3 &operator+=( const Vec3 &o ) noexcept;
constexpr Vec3 &operator-=( const Vec3 &o ) noexcept;
constexpr Vec3 &operator*=( vec_t s )       noexcept;
```

The free-function forms (`dot`, `cross`, `length`, `normalize`) remain canonical
for generic code that may operate on any vector type.

## Vector free functions

```cpp
constexpr vec_t dot      ( const Vec3 &a, const Vec3 &b ) noexcept;
constexpr Vec3  cross    ( const Vec3 &a, const Vec3 &b ) noexcept;
inline    float length   ( const Vec3 &v ) noexcept;
inline    Vec3  normalize( const Vec3 &v ) noexcept;  // zero-vec if length < epsilon
```

`Vec3` arithmetic operators (`+`, `-`, `*` by scalar) are `constexpr` free
functions. `dot` and `cross` are also available for `Vec2` and `Vec4`.

## Angle utilities

```cpp
// Named return type — replaces the output-pointer form of legacy AngleVectors.
struct AngleVectors { Vec3 fwd, right, up; };

// Decompose Euler angles (deg) into forward / right / up unit vectors.
// Legacy: AngleVectors (output-pointer form — replaced by this struct return).
AngleVectors angle_vectors( const Vec3 &angles ) noexcept;

// Forward vector → yaw angle (deg).
// Legacy: VecToYaw / SV_VecToYaw
float vec_to_yaw( const Vec3 &v ) noexcept;

// Forward vector → pitch+yaw Euler angles (deg).
// Legacy: VectorAngles
Vec3 vector_angles( const Vec3 &fwd ) noexcept;
```

These mirror the legacy `AngleVectors`, `VectorAngles`, and `VecToYaw` from
`matrixlib.c`. The argument order and sign conventions are identical.
`AngleVectors` is now a named-field struct rather than output pointer
parameters, eliminating nullable-pointer ambiguity at call sites.

## Matrix3x4 — affine transforms

```cpp
struct Matrix3x4 {
    std::array<std::array<float, 4>, 3> m{};
    static Matrix3x4 identity() noexcept;
    const float *data() const noexcept;  // pointer to m[0][0] for C APIs
    float       *data()       noexcept;
};
```

A 3×4 row-major matrix representing an affine transform (rotation, scale, and
translation). The fourth column carries the translation. The underlying storage
is `std::array<std::array<float,4>,3>` — identical memory layout to the legacy
`float[3][4]` used throughout the renderer and studio model code. Use `.data()`
to obtain a `float*` for C API boundaries.

### Free functions

```cpp
// Apply affine transform to a point (includes translation).
Vec3 transform_point( const Matrix3x4 &m, Vec3 p ) noexcept;

// Rotate a direction vector (ignores translation column).
Vec3 rotate_vector( const Matrix3x4 &m, Vec3 v ) noexcept;

// Matrix concatenation: a * b
Matrix3x4 concat( const Matrix3x4 &a, const Matrix3x4 &b ) noexcept;

// Invert an orthonormal matrix (transpose + translation adjust).
Matrix3x4 invert_ortho( const Matrix3x4 &m ) noexcept;

// Build a Matrix3x4 from an origin point and Euler angles (deg).
Matrix3x4 from_angles( const Vec3 &origin, const Vec3 &angles ) noexcept;
```

`invert_ortho` is valid only for matrices without non-uniform scale. It uses the
transpose shortcut (faster than full Cramer's rule inversion). For matrices that
may have non-uniform scale, a full inverse is needed — this is not currently
provided.

### Operator overloads

```cpp
Matrix3x4 operator*( const Matrix3x4 &a, const Matrix3x4 &b ) noexcept;
Vec3      operator*( const Matrix3x4 &m, Vec3 p ) noexcept;  // calls transform_point
```

## Matrix4x4 — projective transforms

```cpp
struct Matrix4x4 {
    std::array<std::array<float, 4>, 4> m{};
    static Matrix4x4 identity() noexcept;
    const float *data() const noexcept;
    float       *data()       noexcept;
};
```

A 4×4 row-major matrix for full projective transforms (perspective and
orthographic projections). Bone-transform composition and viewport projection
matrices are built here.

### Free functions

```cpp
// Concatenate two projective matrices.
Matrix4x4 concat( const Matrix4x4 &a, const Matrix4x4 &b ) noexcept;

// Transform a 3-D point (homogeneous divide).
Vec3 transform_coord( const Matrix4x4 &m, Vec3 p ) noexcept;

// Build a perspective projection matrix (FOV, aspect, near, far planes).
Matrix4x4 perspective( float fov, float aspect, float near_z, float far_z ) noexcept;

// Build a look-at view matrix.
Matrix4x4 look_at( Vec3 eye, Vec3 target, Vec3 up ) noexcept;

// Extract the top-left 3×4 block as a Matrix3x4.
Matrix3x4 to_matrix3x4( const Matrix4x4 &m ) noexcept;
```

### Operator overloads

```cpp
Matrix4x4 operator*( const Matrix4x4 &a, const Matrix4x4 &b ) noexcept;
Vec3      operator*( const Matrix4x4 &m, Vec3 p ) noexcept;  // calls transform_coord
```

## C API interop

Legacy renderer and studio-model code passes `float *` to functions that operate
on matrix data. Both `Matrix3x4` and `Matrix4x4` expose a `.data()` accessor
that returns a `float*` pointing to the first element. Use this at ABI
boundaries rather than `&m.m[0][0]` directly.

## Thread safety

All functions are pure transformations over value-type arguments. There is no
shared mutable state.
