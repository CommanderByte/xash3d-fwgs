// xash3dpp — studio bone-math primitive tests (Phase A)
// Covers utilities::{angle_quaternion_studio, quaternion_slerp, from_origin_quat,
//   angles_from_matrix, slerp_bones} and matrix::create_from_entity.
//
// Two tiers (the project goldens convention):
//   • hand-derived EXACT cases — dyadic quaternions, identity/branch-literal
//     matrices, near-parallel/align slerp — checkable by hand with no trig.
//   • generator-backed cases — bit-exact values from the verbatim legacy kernels
//     (tests/goldens/studio_math_goldens.inc, produced by tools/legacy_golden_gen)
//     for the trig-interior paths. Compared with exact ==: the port is a faithful
//     transcription, so any 1-ULP drift is a real parity bug, not tolerance noise.

#include <xash3dpp/utilities/quaternion.hpp>
#include <xash3dpp/utilities/matrix.hpp>

#include <cstdio>

#include "../test_helpers.hpp"
#include "../goldens/studio_math_goldens.inc"

static int g_pass = 0, g_fail = 0;

using xash::utilities::Matrix3x4;
using xash::utilities::Vec3;
using xash::utilities::Vec4;

// Exact bit comparison — the goldens are bit-exact legacy values.
static bool vec4_eq( const Vec4 &v, const float g[4] ) noexcept
{
    return v.x == g[0] && v.y == g[1] && v.z == g[2] && v.w == g[3];
}
static bool vec3_eq( const Vec3 &v, const float g[3] ) noexcept
{
    return v.x == g[0] && v.y == g[1] && v.z == g[2];
}
static bool mat_eq( const Matrix3x4 &m, const float g[12] ) noexcept
{
    for( int r = 0, i = 0; r < 3; ++r )
        for( int c = 0; c < 4; ++c, ++i )
            if( m.m[r][c] != g[i] ) return false;
    return true;
}
static Matrix3x4 mat_from( const float g[12] ) noexcept
{
    Matrix3x4 m{};
    for( int r = 0, i = 0; r < 3; ++r )
        for( int c = 0; c < 4; ++c, ++i )
            m.m[r][c] = g[i];
    return m;
}

// ---------------------------------------------------------------------------
// Hand-derived exact cases
// ---------------------------------------------------------------------------

static void test_angle_quaternion_identity()
{
    // sincos(0) = {0,1} for all three -> q = {0,0,0,1} exactly.
    const Vec4 q = xash::utilities::angle_quaternion_studio( { 0.0f, 0.0f, 0.0f } );
    CHECK( q.x == 0.0f && q.y == 0.0f && q.z == 0.0f && q.w == 1.0f );
}

static void test_from_origin_quat_dyadic()
{
    using xash::utilities::from_origin_quat;

    // identity quaternion -> identity rotation + origin.
    const Matrix3x4 mi = from_origin_quat( { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 2.0f, 3.0f } );
    CHECK( mi.m[0][0] == 1.0f && mi.m[0][1] == 0.0f && mi.m[0][2] == 0.0f && mi.m[0][3] == 1.0f );
    CHECK( mi.m[1][0] == 0.0f && mi.m[1][1] == 1.0f && mi.m[1][2] == 0.0f && mi.m[1][3] == 2.0f );
    CHECK( mi.m[2][0] == 0.0f && mi.m[2][1] == 0.0f && mi.m[2][2] == 1.0f && mi.m[2][3] == 3.0f );

    // {1,0,0,0} -> 180 deg about X -> diag(1,-1,-1).
    const Matrix3x4 mx = from_origin_quat( { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } );
    CHECK( mx.m[0][0] == 1.0f && mx.m[1][1] == -1.0f && mx.m[2][2] == -1.0f );
    CHECK( mx.m[0][1] == 0.0f && mx.m[0][2] == 0.0f && mx.m[1][0] == 0.0f
        && mx.m[1][2] == 0.0f && mx.m[2][0] == 0.0f && mx.m[2][1] == 0.0f );

    // {0.5,0.5,0.5,0.5} -> cyclic axis permutation (all products dyadic -> exact).
    const Matrix3x4 mp = from_origin_quat( { 0.5f, 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f } );
    CHECK( mp.m[0][0] == 0.0f && mp.m[0][1] == 0.0f && mp.m[0][2] == 1.0f );
    CHECK( mp.m[1][0] == 1.0f && mp.m[1][1] == 0.0f && mp.m[1][2] == 0.0f );
    CHECK( mp.m[2][0] == 0.0f && mp.m[2][1] == 1.0f && mp.m[2][2] == 0.0f );
}

static void test_slerp_identical_and_align()
{
    using xash::utilities::quaternion_slerp;
    const Vec4 p{ 0.5f, 0.5f, 0.5f, 0.5f };

    // identical quats -> near-parallel branch: qt = (1-t)*p + t*p = p (dyadic t).
    for( float t : { 0.0f, 0.25f, 0.5f, 1.0f } )
    {
        const Vec4 r = quaternion_slerp( p, p, t );
        CHECK( r.x == 0.5f && r.y == 0.5f && r.z == 0.5f && r.w == 0.5f );
    }

    // align flip: slerp(p, -p, 0.5) aligns -p back to p, then returns p.
    const Vec4 neg{ -0.5f, -0.5f, -0.5f, -0.5f };
    const Vec4 ra = quaternion_slerp( p, neg, 0.5f );
    CHECK( ra.x == 0.5f && ra.y == 0.5f && ra.z == 0.5f && ra.w == 0.5f );
}

static void test_create_from_entity_hand()
{
    using xash::utilities::create_from_entity;

    // identity branch: diag(scale) + origin, exact zeros elsewhere.
    const Matrix3x4 mi = create_from_entity( { 1.0f, 2.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, 1.5f );
    CHECK( mi.m[0][0] == 1.5f && mi.m[0][1] == 0.0f && mi.m[0][2] == 0.0f && mi.m[0][3] == 1.0f );
    CHECK( mi.m[1][0] == 0.0f && mi.m[1][1] == 1.5f && mi.m[1][2] == 0.0f && mi.m[1][3] == 2.0f );
    CHECK( mi.m[2][0] == 0.0f && mi.m[2][1] == 0.0f && mi.m[2][2] == 1.5f && mi.m[2][3] == 3.0f );

    // yaw branch (pitch==roll==0): the literal 0 / scale slots are trig-free.
    const Matrix3x4 my = create_from_entity( { 7.0f, 8.0f, 9.0f }, { 0.0f, 20.0f, 0.0f }, 1.0f );
    CHECK( my.m[0][2] == 0.0f && my.m[1][2] == 0.0f );
    CHECK( my.m[2][0] == 0.0f && my.m[2][1] == 0.0f && my.m[2][2] == 1.0f );
    CHECK( my.m[0][3] == 7.0f && my.m[1][3] == 8.0f && my.m[2][3] == 9.0f );

    // from_angles is create_from_entity at scale 1 — bit-identical.
    const Matrix3x4 fa = xash::utilities::from_angles( { 7.0f, 8.0f, 9.0f }, { 0.0f, 20.0f, 0.0f } );
    bool fa_same = true;
    for( int r = 0; r < 3; ++r )
        for( int c = 0; c < 4; ++c )
            if( fa.m[r][c] != my.m[r][c] ) fa_same = false;
    CHECK( fa_same );
}

static void test_slerp_bones_hand()
{
    // 2 bones, s = 0.5, dyadic -> exact per-bone slerp(identical) + lerp.
    Vec4 q1[2] = { { 0.5f, 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
    Vec3 p1[2] = { { 0.0f, 0.0f, 0.0f }, { 2.0f, 4.0f, 6.0f } };
    const Vec4 q2[2] = { { 0.5f, 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
    const Vec3 p2[2] = { { 2.0f, 2.0f, 2.0f }, { 4.0f, 4.0f, 4.0f } };

    xash::utilities::slerp_bones( q1, p1, q2, p2, 0.5f );

    // orientations unchanged (identical), positions lerped by 0.5.
    CHECK( q1[0].x == 0.5f && q1[0].w == 0.5f );
    CHECK( q1[1].w == 1.0f );
    CHECK( p1[0].x == 1.0f && p1[0].y == 1.0f && p1[0].z == 1.0f ); // 0 + .5*(2-0)
    CHECK( p1[1].x == 3.0f && p1[1].y == 4.0f && p1[1].z == 5.0f ); // 2+.5*2, 4, 6+.5*-2
}

// ---------------------------------------------------------------------------
// Generator-backed bit-exact cross-check vs the verbatim legacy kernels
// ---------------------------------------------------------------------------

static void test_golden_angle_quaternion()
{
    for( const auto &c : xash::goldens::k_angle_quaternion_studio )
    {
        const Vec4 q = xash::utilities::angle_quaternion_studio(
            { c.angles_rad[0], c.angles_rad[1], c.angles_rad[2] } );
        CHECK( vec4_eq( q, c.q ) );
    }
}

static void test_golden_slerp()
{
    for( const auto &c : xash::goldens::k_quaternion_slerp )
    {
        const Vec4 p{ c.p[0], c.p[1], c.p[2], c.p[3] };
        const Vec4 q{ c.q[0], c.q[1], c.q[2], c.q[3] };
        const Vec4 r = xash::utilities::quaternion_slerp( p, q, c.t );
        CHECK( vec4_eq( r, c.out ) );
    }
}

static void test_golden_create_from_entity()
{
    for( const auto &c : xash::goldens::k_create_from_entity )
    {
        const Matrix3x4 m = xash::utilities::create_from_entity(
            { c.origin[0], c.origin[1], c.origin[2] },
            { c.angles_deg[0], c.angles_deg[1], c.angles_deg[2] }, c.scale );
        CHECK( mat_eq( m, c.m ) );
    }
}

static void test_golden_from_origin_quat()
{
    for( const auto &c : xash::goldens::k_from_origin_quat )
    {
        const Matrix3x4 m = xash::utilities::from_origin_quat(
            { c.q[0], c.q[1], c.q[2], c.q[3] }, { c.origin[0], c.origin[1], c.origin[2] } );
        CHECK( mat_eq( m, c.m ) );
    }
}

static void test_golden_angles_from_matrix()
{
    for( const auto &c : xash::goldens::k_angles_from_matrix )
    {
        const Vec3 a = xash::utilities::angles_from_matrix( mat_from( c.m ) );
        CHECK( vec3_eq( a, c.angles ) );
    }
}

int main()
{
    RUN_TEST( test_angle_quaternion_identity );
    RUN_TEST( test_from_origin_quat_dyadic );
    RUN_TEST( test_slerp_identical_and_align );
    RUN_TEST( test_create_from_entity_hand );
    RUN_TEST( test_slerp_bones_hand );

    RUN_TEST( test_golden_angle_quaternion );
    RUN_TEST( test_golden_slerp );
    RUN_TEST( test_golden_create_from_entity );
    RUN_TEST( test_golden_from_origin_quat );
    RUN_TEST( test_golden_angles_from_matrix );

    std::printf( "test_quaternion: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
