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
float rint  ( float f ) noexcept;   // round to nearest integer
bool  is_nan( float f ) noexcept;   // IEEE 754 NaN check
```

`rint` rounds half-values away from zero (same behaviour as the legacy `rint`
macro in `crtlib.h`). `is_nan` uses `f != f` to detect NaN without including
`<cmath>` in headers.

## Vector free functions

```cpp
float dot  ( Vec3 a, Vec3 b ) noexcept;
Vec3  cross( Vec3 a, Vec3 b ) noexcept;
float length   ( Vec3 v ) noexcept;
Vec3  normalize( Vec3 v ) noexcept;    // returns zero-vec if length < epsilon
```

All `Vec3` operators (`+`, `-`, `*`, `/`, unary `-`) are defined as `inline`
free functions. `dot` and `cross` are also available for `Vec2` and `Vec4`.

## Angle utilities

```cpp
// Decompose an Euler angle vector into three orthogonal unit vectors.
void AngleVectors( Vec3 angles, Vec3 *fwd, Vec3 *right, Vec3 *up ) noexcept;

// Compute a forward vector from an Euler angle (pitch-yaw-roll).
Vec3 angle_vectors( Vec3 fwd ) noexcept;

// Compute the yaw angle (in degrees) that points along v.
float vec_to_yaw( Vec3 v ) noexcept;

// Compute Euler angles from a forward vector and optional up vector.
Vec3 vector_angles( Vec3 fwd, Vec3 up = {} ) noexcept;
```

These mirror the legacy `AngleVectors`, `VectorAngles`, and `VecToYaw` from
`matrixlib.c`. The argument order and sign conventions are identical.

## Matrix3x4 — affine transforms

```cpp
struct Matrix3x4 { float m[3][4]; };
```

A 3×4 row-major matrix representing an affine transform (rotation, scale, and
translation). The fourth column carries the translation. The underlying storage
`float m[3][4]` is contiguous and interoperable with the legacy `matrix3x4`
type used throughout the renderer and studio model code.

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

// Build a Matrix3x4 from Euler angles (no scale, no translation).
Matrix3x4 from_angles( Vec3 angles ) noexcept;
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
struct Matrix4x4 { float m[4][4]; };
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
on matrix data. `Matrix3x4::m` and `Matrix4x4::m` are accessible directly as
`float[3][4]` / `float[4][4]` and can be passed to legacy pointers with an
explicit `&m.m[0][0]`. There is no `.data()` method; use this pattern only at
ABI boundaries.

## Thread safety

All functions are pure transformations over value-type arguments. There is no
shared mutable state.
