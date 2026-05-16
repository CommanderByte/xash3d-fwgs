// xash3dpp — math utility tests
// Covers: rint, is_nan, dot, cross, length, normalize,
//         Vec3 operators (+, -, *, +=, -=, *=),
//         angle_vectors, vec_to_yaw, vector_angles

#include <xash3dpp/utilities/math.hpp>
#include <cstdio>
#include <limits>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

static bool near( float a, float b, float tol = 1e-4f ) noexcept
{
    const float d = a - b;
    return ( d < 0.0f ? -d : d ) <= tol;
}

static bool near3( const xash::utilities::Vec3& a, const xash::utilities::Vec3& b,
                   float tol = 1e-4f ) noexcept
{
    return near( a.x, b.x, tol ) && near( a.y, b.y, tol ) && near( a.z, b.z, tol );
}

// ---------------------------------------------------------------------------
// rint
// ---------------------------------------------------------------------------

static void test_rint()
{
    // legacy: Q_rint macro in public/xash3d_mathlib.h
    CHECK( xash::utilities::rint(  0.0f ) ==  0.0f );
    CHECK( xash::utilities::rint(  1.4f ) ==  1.0f );
    CHECK( xash::utilities::rint(  1.5f ) ==  2.0f );  // rounds up at exactly .5
    CHECK( xash::utilities::rint(  2.7f ) ==  3.0f );
    CHECK( xash::utilities::rint( -0.4f ) ==  0.0f );  // int(-0.9) truncates toward 0
    CHECK( xash::utilities::rint( -0.5f ) == -1.0f );  // int(-1.0) = -1
    CHECK( xash::utilities::rint( -1.4f ) == -1.0f );  // int(-1.9) truncates toward 0 → -1
    CHECK( xash::utilities::rint( -1.5f ) == -2.0f );  // int(-2.0) = -2
    CHECK( xash::utilities::rint( -2.7f ) == -3.0f );
}

// ---------------------------------------------------------------------------
// is_nan
// ---------------------------------------------------------------------------

static void test_is_nan()
{
    CHECK( !xash::utilities::is_nan(  0.0f ) );
    CHECK( !xash::utilities::is_nan(  1.0f ) );
    CHECK( !xash::utilities::is_nan( -1.0f ) );
    CHECK(  xash::utilities::is_nan( std::numeric_limits<float>::quiet_NaN() ) );
    CHECK( !xash::utilities::is_nan( std::numeric_limits<float>::infinity() ) );
}

// ---------------------------------------------------------------------------
// dot (free function)
// ---------------------------------------------------------------------------

static void test_dot()
{
    // legacy: DotProduct macro in public/xash3d_mathlib.h
    CHECK( xash::utilities::dot( {1,0,0}, {1,0,0}  ) ==  1.0f );
    CHECK( xash::utilities::dot( {1,0,0}, {0,1,0}  ) ==  0.0f );  // orthogonal
    CHECK( xash::utilities::dot( {1,0,0}, {-1,0,0} ) == -1.0f );  // antiparallel
    CHECK( xash::utilities::dot( {1,2,3}, {4,5,6}  ) == 32.0f );  // 4+10+18
}

// ---------------------------------------------------------------------------
// cross
// ---------------------------------------------------------------------------

static void test_cross()
{
    // legacy: CrossProduct macro in public/xash3d_mathlib.h
    // Right-hand basis: X×Y = Z
    CHECK( near3( xash::utilities::cross( {1,0,0}, {0,1,0} ), {0,0,1}  ) );
    // Anticommutative: Y×X = -Z
    CHECK( near3( xash::utilities::cross( {0,1,0}, {1,0,0} ), {0,0,-1} ) );
    // Self-cross = zero vector.
    CHECK( near3( xash::utilities::cross( {1,2,3}, {1,2,3} ), {0,0,0}  ) );
}

// ---------------------------------------------------------------------------
// Vec3 arithmetic operators
// ---------------------------------------------------------------------------

static void test_vec3_operators()
{
    using V = xash::utilities::Vec3;
    const V a{1,2,3}, b{4,5,6};

    CHECK( near3( a + b, {5,7,9} ) );
    CHECK( near3( b - a, {3,3,3} ) );
    CHECK( near3( a * 2.0f, {2,4,6} ) );
    CHECK( near3( 2.0f * a, {2,4,6} ) );  // commutative scalar-multiply

    // Compound-assign operators.
    V c{1,2,3};
    c += {1,1,1};
    CHECK( near3( c, {2,3,4} ) );
    c -= {1,1,1};
    CHECK( near3( c, {1,2,3} ) );
    c *= 3.0f;
    CHECK( near3( c, {3,6,9} ) );
}

// ---------------------------------------------------------------------------
// length / normalize (free functions)
// ---------------------------------------------------------------------------

static void test_length()
{
    // legacy: VectorLength in public/xash3d_mathlib.h
    CHECK( near( xash::utilities::length( {3,4,0} ), 5.0f ) );
    CHECK( near( xash::utilities::length( {0,0,0} ), 0.0f ) );
    CHECK( near( xash::utilities::length( {1,0,0} ), 1.0f ) );
    CHECK( near( xash::utilities::length( {1,1,1} ), 1.7321f ) );  // sqrt(3)
}

static void test_normalize()
{
    // legacy: VectorNormalize in public/xash3d_mathlib.h
    const auto n = xash::utilities::normalize( {3,4,0} );
    CHECK( near( n.x, 0.6f ) && near( n.y, 0.8f ) && near( n.z, 0.0f ) );

    // Unit vector normalised to itself.
    const auto u = xash::utilities::normalize( {0,0,1} );
    CHECK( near3( u, {0,0,1} ) );

    // Zero vector → zero vector (no division by zero / UB).
    const auto z = xash::utilities::normalize( {0,0,0} );
    CHECK( near3( z, {0,0,0} ) );
}

// ---------------------------------------------------------------------------
// angle_vectors
// ---------------------------------------------------------------------------

static void test_angle_vectors()
{
    // legacy: AngleVectors in public/xash3d_mathlib.c (PITCH/YAW/ROLL)
    // Identity angles: forward=+X, right=-Y, up=+Z.
    {
        const auto av = xash::utilities::angle_vectors( {0,0,0} );
        CHECK( near3( av.fwd,   {1,0,0}  ) );
        CHECK( near3( av.right, {0,-1,0} ) );
        CHECK( near3( av.up,    {0,0,1}  ) );
    }
    // Yaw=90 → forward points along +Y.
    {
        const auto av = xash::utilities::angle_vectors( {0,90,0} );
        CHECK( near3( av.fwd, {0,1,0} ) );
    }
    // Pitch=90 → forward points along -Z (down in Quake convention).
    {
        const auto av = xash::utilities::angle_vectors( {90,0,0} );
        CHECK( near3( av.fwd, {0,0,-1} ) );
    }
}

// ---------------------------------------------------------------------------
// vec_to_yaw
// ---------------------------------------------------------------------------

static void test_vec_to_yaw()
{
    // legacy: VecToYaw / SV_VecToYaw
    CHECK( near( xash::utilities::vec_to_yaw( { 1, 0, 0} ),   0.0f ) );  // +X → 0°
    CHECK( near( xash::utilities::vec_to_yaw( { 0, 1, 0} ),  90.0f ) );  // +Y → 90°
    CHECK( near( xash::utilities::vec_to_yaw( {-1, 0, 0} ), 180.0f ) );  // -X → 180°
    CHECK( near( xash::utilities::vec_to_yaw( { 0,-1, 0} ), 270.0f ) );  // -Y → 270°
    CHECK( near( xash::utilities::vec_to_yaw( { 0, 0, 1} ),   0.0f ) );  // pure Z → 0° (special case)
}

// ---------------------------------------------------------------------------
// vector_angles
// ---------------------------------------------------------------------------

static void test_vector_angles()
{
    // legacy: VectorAngles — converts a forward direction to Euler angles.
    // +X axis → pitch=0, yaw=0.
    {
        const auto a = xash::utilities::vector_angles( {1,0,0} );
        CHECK( near( a.x, 0.0f ) && near( a.y, 0.0f ) );
    }
    // +Y axis → pitch=0, yaw=90.
    {
        const auto a = xash::utilities::vector_angles( {0,1,0} );
        CHECK( near( a.x, 0.0f ) && near( a.y, 90.0f ) );
    }
    // +Z axis (straight up) → pitch=90.
    {
        const auto a = xash::utilities::vector_angles( {0,0,1} );
        CHECK( near( a.x, 90.0f ) );
    }
    // -Z axis (straight down) → pitch=270.
    {
        const auto a = xash::utilities::vector_angles( {0,0,-1} );
        CHECK( near( a.x, 270.0f ) );
    }
    // Roll component is always 0.
    {
        const auto a = xash::utilities::vector_angles( {1,1,0} );
        CHECK( near( a.z, 0.0f ) );
    }
}

int main()
{
    RUN_TEST( test_rint );
    RUN_TEST( test_is_nan );
    RUN_TEST( test_dot );
    RUN_TEST( test_cross );
    RUN_TEST( test_vec3_operators );
    RUN_TEST( test_length );
    RUN_TEST( test_normalize );
    RUN_TEST( test_angle_vectors );
    RUN_TEST( test_vec_to_yaw );
    RUN_TEST( test_vector_angles );

    std::printf( "math: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
