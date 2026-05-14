// xash3dpp — matrix and math tests
// Covers: Vec3 (dot, cross, normalize, length), rint, is_nan,
//         Matrix3x4 (identity, transform_point, rotate_vector, concat,
//                    invert_ortho, from_angles),
//         Matrix4x4 (identity, concat, transform_coord),
//         angle_vectors, vec_to_yaw, vector_angles

#include <xash3dpp/utilities/matrix.hpp>

#include <cstdio>
#include <limits>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::puts("FAIL: " #expr " (" __FILE__ ")"); } } while(0)

static bool near( float a, float b, float tol = 1e-4f ) noexcept
{
    const float d = a - b;
    return ( d < 0.0f ? -d : d ) <= tol;
}

// ---------------------------------------------------------------------------
// Vec3 math (inline / constexpr — no linker dep)
// ---------------------------------------------------------------------------

static void test_dot()
{
    // legacy: DotProduct macro in public/xash3d_mathlib.h
    CHECK( xash::utilities::dot( {1,0,0}, {0,1,0} ) == 0.0f );
    CHECK( xash::utilities::dot( {1,0,0}, {1,0,0} ) == 1.0f );
    CHECK( xash::utilities::dot( {3,4,0}, {3,4,0} ) == 25.0f );
    CHECK( xash::utilities::dot( {1,2,3}, {4,5,6} ) == 32.0f );  // 4+10+18
}

static void test_cross()
{
    // legacy: CrossProduct macro
    const auto c1 = xash::utilities::cross( {1,0,0}, {0,1,0} );
    CHECK( c1.x == 0.0f && c1.y == 0.0f && c1.z == 1.0f );

    const auto c2 = xash::utilities::cross( {0,1,0}, {0,0,1} );
    CHECK( c2.x == 1.0f && c2.y == 0.0f && c2.z == 0.0f );

    // Anti-commutativity: a×b == -(b×a)
    const auto ab = xash::utilities::cross( {1,2,3}, {4,5,6} );
    const auto ba = xash::utilities::cross( {4,5,6}, {1,2,3} );
    CHECK( near( ab.x, -ba.x ) && near( ab.y, -ba.y ) && near( ab.z, -ba.z ) );
}

static void test_normalize()
{
    const auto n = xash::utilities::normalize( {3,0,4} );
    CHECK( near( n.x, 0.6f ) && n.y == 0.0f && near( n.z, 0.8f ) );

    // Normalized vector has unit length.
    CHECK( near( xash::utilities::length( n ), 1.0f ) );

    // Zero vector → zero (no crash, no NaN)
    const auto nz = xash::utilities::normalize( {0,0,0} );
    CHECK( nz.x == 0.0f && nz.y == 0.0f && nz.z == 0.0f );
}

static void test_rint()
{
    // legacy: rint macro — round half away from zero
    CHECK( xash::utilities::rint(  1.6f ) ==  2.0f );
    CHECK( xash::utilities::rint(  1.4f ) ==  1.0f );
    CHECK( xash::utilities::rint(  0.5f ) ==  1.0f );
    CHECK( xash::utilities::rint( -1.6f ) == -2.0f );
    CHECK( xash::utilities::rint( -1.4f ) == -1.0f );
    CHECK( xash::utilities::rint(  0.0f ) ==  0.0f );
}

static void test_is_nan()
{
    // legacy: IS_NAN macro — relies on NaN != NaN property
    CHECK( !xash::utilities::is_nan(  1.0f ) );
    CHECK( !xash::utilities::is_nan( -1.0f ) );
    CHECK( !xash::utilities::is_nan(  0.0f ) );
    CHECK(  xash::utilities::is_nan( std::numeric_limits<float>::quiet_NaN() ) );
}

// ---------------------------------------------------------------------------
// Matrix3x4
// ---------------------------------------------------------------------------

static void test_matrix3x4_identity()
{
    const auto id = xash::utilities::Matrix3x4::identity();

    // Identity transform: any point maps to itself.
    const auto v = xash::utilities::transform_point( id, {3,4,5} );
    CHECK( v.x == 3.0f && v.y == 4.0f && v.z == 5.0f );

    // Identity rotate: any vector maps to itself.
    const auto r = xash::utilities::rotate_vector( id, {1,2,3} );
    CHECK( r.x == 1.0f && r.y == 2.0f && r.z == 3.0f );
}

static void test_from_angles_translation()
{
    // from_angles with zero angles: rotation = identity, translation applied.
    const auto m = xash::utilities::from_angles( {1,2,3}, {0,0,0} );
    const auto pt = xash::utilities::transform_point( m, {0,0,0} );
    CHECK( near( pt.x, 1.0f ) && near( pt.y, 2.0f ) && near( pt.z, 3.0f ) );

    // rotate_vector ignores the translation column.
    const auto rv = xash::utilities::rotate_vector( m, {1,0,0} );
    CHECK( near( rv.x, 1.0f ) && near( rv.y, 0.0f ) && near( rv.z, 0.0f ) );
}

static void test_from_angles_yaw()
{
    // Yaw 90° rotates {1,0,0} → {0,1,0} in the XY plane.
    const auto m = xash::utilities::from_angles( {0,0,0}, {0,90,0} );
    const auto v = xash::utilities::transform_point( m, {1,0,0} );
    CHECK( near( v.x, 0.0f ) && near( v.y, 1.0f ) && near( v.z, 0.0f ) );
}

static void test_concat_identity()
{
    // id * id == id (transform_point result unchanged)
    const auto id = xash::utilities::Matrix3x4::identity();
    const auto cc = xash::utilities::concat( id, id );
    const auto v  = xash::utilities::transform_point( cc, {7,8,9} );
    CHECK( near( v.x, 7.0f ) && near( v.y, 8.0f ) && near( v.z, 9.0f ) );
}

static void test_invert_ortho()
{
    // Rotate by yaw 90°, then invert → should get back original point.
    const auto m  = xash::utilities::from_angles( {0,0,0}, {0,90,0} );
    const auto mi = xash::utilities::invert_ortho( m );
    const auto v  = xash::utilities::transform_point( m, {1,0,0} );    // → {0,1,0}
    const auto vv = xash::utilities::transform_point( mi, v );           // → {1,0,0}
    CHECK( near( vv.x, 1.0f ) && near( vv.y, 0.0f ) && near( vv.z, 0.0f ) );

    // Identity inverts to identity.
    const auto id  = xash::utilities::Matrix3x4::identity();
    const auto idi = xash::utilities::invert_ortho( id );
    const auto u   = xash::utilities::transform_point( idi, {5,6,7} );
    CHECK( near( u.x, 5.0f ) && near( u.y, 6.0f ) && near( u.z, 7.0f ) );
}

// ---------------------------------------------------------------------------
// Matrix4x4
// ---------------------------------------------------------------------------

static void test_matrix4x4_identity()
{
    const auto id = xash::utilities::Matrix4x4::identity();
    const auto v  = xash::utilities::transform_coord( id, {3,4,5} );
    CHECK( near( v.x, 3.0f ) && near( v.y, 4.0f ) && near( v.z, 5.0f ) );
}

static void test_matrix4x4_concat()
{
    const auto id = xash::utilities::Matrix4x4::identity();
    const auto cc = xash::utilities::concat( id, id );
    const auto v  = xash::utilities::transform_coord( cc, {1,2,3} );
    CHECK( near( v.x, 1.0f ) && near( v.y, 2.0f ) && near( v.z, 3.0f ) );
}

// ---------------------------------------------------------------------------
// Angle helpers
// ---------------------------------------------------------------------------

static void test_angle_vectors()
{
    // legacy: AngleVectors in public/matrixlib.c — zero angles
    const auto av0 = xash::utilities::angle_vectors( {0,0,0} );
    CHECK( near( av0.fwd.x,  1.0f ) && near( av0.fwd.y,  0.0f ) && near( av0.fwd.z,  0.0f ) );
    CHECK( near( av0.right.x, 0.0f ) && near( av0.right.y, -1.0f ) && near( av0.right.z, 0.0f ) );
    CHECK( near( av0.up.x,   0.0f ) && near( av0.up.y,   0.0f ) && near( av0.up.z,   1.0f ) );

    // Yaw 90° looks along +Y.
    const auto av90 = xash::utilities::angle_vectors( {0,90,0} );
    CHECK( near( av90.fwd.x, 0.0f ) && near( av90.fwd.y, 1.0f ) && near( av90.fwd.z, 0.0f ) );
    CHECK( near( av90.right.x, 1.0f ) && near( av90.right.y, 0.0f ) && near( av90.right.z, 0.0f ) );
    CHECK( near( av90.up.x, 0.0f ) && near( av90.up.y, 0.0f ) && near( av90.up.z, 1.0f ) );
}

static void test_vec_to_yaw()
{
    // legacy: VecToYaw
    CHECK( near( xash::utilities::vec_to_yaw( {1,0,0} ),   0.0f ) );
    CHECK( near( xash::utilities::vec_to_yaw( {0,1,0} ),  90.0f ) );
    CHECK( near( xash::utilities::vec_to_yaw( {-1,0,0} ), 180.0f ) );
    // {0,-1,0} → atan2(-1,0) = -90° → +270°
    CHECK( near( xash::utilities::vec_to_yaw( {0,-1,0} ), 270.0f ) );
    // Degenerate zero vector → 0.
    CHECK( near( xash::utilities::vec_to_yaw( {0,0,0} ), 0.0f ) );
}

static void test_vector_angles()
{
    // legacy: VectorAngles
    const auto a0 = xash::utilities::vector_angles( {1,0,0} );
    CHECK( near( a0.x, 0.0f ) && near( a0.y, 0.0f ) );

    // Pointing along +Y → yaw = 90.
    const auto a1 = xash::utilities::vector_angles( {0,1,0} );
    CHECK( near( a1.x, 0.0f ) && near( a1.y, 90.0f ) );

    // Straight up → pitch = 90, yaw = 0.
    const auto a2 = xash::utilities::vector_angles( {0,0,1} );
    CHECK( near( a2.x, 90.0f ) && near( a2.y, 0.0f ) );

    // Straight down → pitch = 270, yaw = 0.
    const auto a3 = xash::utilities::vector_angles( {0,0,-1} );
    CHECK( near( a3.x, 270.0f ) && near( a3.y, 0.0f ) );
}

int main()
{
    test_dot();
    test_cross();
    test_normalize();
    test_rint();
    test_is_nan();
    test_matrix3x4_identity();
    test_from_angles_translation();
    test_from_angles_yaw();
    test_concat_identity();
    test_invert_ortho();
    test_matrix4x4_identity();
    test_matrix4x4_concat();
    test_angle_vectors();
    test_vec_to_yaw();
    test_vector_angles();

    std::printf( "matrix: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
