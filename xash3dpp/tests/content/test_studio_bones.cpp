// xash3dpp — studio bone kernel tests (Phase B)
// Covers content::{calc_bones, calc_bone_adj} + the studio.hpp offset sub-views.
//
// Parity approach: the RLE position decode is hand-derived EXACTLY (dyadic
// scales, integer raw values) — no trig. The rotation path is checked against
// the golden-verified utilities primitives as an oracle: calc_bones must produce
// angle_quaternion_studio / quaternion_slerp of the SAME decoded angles it
// derives, so the composition is bit-exact without a separate legacy golden
// (the leaf trig is already anchored by tests/goldens/studio_math_goldens.inc).

#include <xash3dpp/content/bone_solver.hpp>
#include <xash3dpp/content/studio.hpp>
#include <xash3dpp/utilities/quaternion.hpp>

#include <array>
#include <cstdio>
#include <vector>

#include "../test_helpers.hpp"
#include "studio_builder.hpp"

static int g_pass = 0, g_fail = 0;

using xash::content::AnimView;
using xash::content::BoneView;
using xash::content::StudioView;
using xash::content::test::anim_num;
using xash::content::test::StudioBuilder;
using xash::utilities::Vec3;
using xash::utilities::Vec4;

static bool q_eq( const Vec4 &a, const Vec4 &b ) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static bool mat_eq( const xash::utilities::Matrix3x4 &a, const xash::utilities::Matrix3x4 &b ) noexcept
{
    for( int r = 0; r < 3; ++r )
        for( int c = 0; c < 4; ++c )
            if( a.m[r][c] != b.m[r][c] ) return false;
    return true;
}

// ---------------------------------------------------------------------------
// calc_bones — RLE position decode (hand-derived exact)
// ---------------------------------------------------------------------------

static void test_calc_bones_rle_position()
{
    StudioBuilder b;
    // value[0]=1 (X base), scale[0]=0.5. Channels 1..5 absent (bone defaults).
    const std::size_t bone_off = b.add_bone(
        -1, { -1, -1, -1, -1, -1, -1 },
        { 1.0f, 2.0f, 3.0f, 0.25f, 0.0f, 0.0f },
        { 0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f } );

    // Channel 0 (X position): two RLE spans, frames 0-1 = {10,20}, 2-3 = {30,40}.
    std::array<std::vector<std::int16_t>, 6> ch{};
    ch[0] = { anim_num( 2, 2 ), 10, 20, anim_num( 2, 2 ), 30, 40 };
    const std::size_t anim_off = b.add_anim( ch );

    const auto &bytes = b.bytes();
    const BoneView bone{ bytes, bone_off };
    const AnimView anim{ bytes, anim_off };

    // decoded raw pairs: frame 0 -> (10,20), frame 1 -> (20,30), frame 2 -> (30,40).
    // v = value[0] + raw*scale[0] = 1 + 0.5*raw.  pos.x = v1 + s*(v2-v1).
    Vec3 pos;
    xash::content::calc_bones( 0, 0.0f, bone, anim, {}, pos, nullptr );
    CHECK( pos.x == 6.0f && pos.y == 2.0f && pos.z == 3.0f ); // 1+.5*10; defaults

    xash::content::calc_bones( 1, 0.5f, bone, anim, {}, pos, nullptr );
    CHECK( pos.x == 13.5f ); // v1=11, v2=16 -> 11+.5*5

    xash::content::calc_bones( 2, 0.0f, bone, anim, {}, pos, nullptr );
    CHECK( pos.x == 16.0f ); // v1=16 (1+.5*30)
}

static void test_calc_bones_position_only_matches_full()
{
    StudioBuilder b;
    const std::size_t bone_off = b.add_bone(
        -1, { -1, -1, -1, -1, -1, -1 },
        { 1.0f, 2.0f, 3.0f, 0.25f, 0.0f, 0.0f },
        { 0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f } );
    std::array<std::vector<std::int16_t>, 6> ch{};
    ch[0] = { anim_num( 2, 2 ), 10, 20, anim_num( 2, 2 ), 30, 40 };
    const std::size_t anim_off = b.add_anim( ch );

    const auto &bytes = b.bytes();
    const BoneView bone{ bytes, bone_off };
    const AnimView anim{ bytes, anim_off };

    Vec3 pos_only, pos_full;
    Vec4 q;
    xash::content::calc_bones( 1, 0.5f, bone, anim, {}, pos_only, nullptr );
    xash::content::calc_bones( 1, 0.5f, bone, anim, {}, pos_full, &q );
    CHECK( pos_only.x == pos_full.x && pos_only.y == pos_full.y && pos_only.z == pos_full.z );

    // rotation channels absent -> v1[3..5]==v2[3..5]==value[3..5]; q is the
    // single AngleQuaternion of that (composition oracle).
    const Vec4 expect = xash::utilities::angle_quaternion_studio( { 0.25f, 0.0f, 0.0f } );
    CHECK( q_eq( q, expect ) );
}

static void test_calc_bones_rotation_slerp_oracle()
{
    StudioBuilder b;
    // rotation X animated: value[3]=0, scale[3]=1; span {0,1} -> raw 0 then 1.
    const std::size_t bone_off = b.add_bone(
        -1, { -1, -1, -1, -1, -1, -1 },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f } );
    std::array<std::vector<std::int16_t>, 6> ch{};
    ch[3] = { anim_num( 2, 2 ), 0, 1 }; // rot-X channel
    const std::size_t anim_off = b.add_anim( ch );

    const auto &bytes = b.bytes();
    const BoneView bone{ bytes, bone_off };
    const AnimView anim{ bytes, anim_off };

    // frame 0: v1[3]=0, v2[3]=1 (distinct) -> slerp(AQ({0,0,0}), AQ({1,0,0}), s).
    for( float s : { 0.0f, 0.25f, 0.5f, 1.0f } )
    {
        Vec3 pos;
        Vec4 q;
        xash::content::calc_bones( 0, s, bone, anim, {}, pos, &q );
        const Vec4 expect = xash::utilities::quaternion_slerp(
            xash::utilities::angle_quaternion_studio( { 0.0f, 0.0f, 0.0f } ),
            xash::utilities::angle_quaternion_studio( { 1.0f, 0.0f, 0.0f } ), s );
        CHECK( q_eq( q, expect ) );
    }
}

static void test_calc_bones_controller_adj()
{
    // A bone whose channel 0 is absent but has bonecontroller[0]=slot 0; the
    // adj for that slot shifts the default value.  value[0]=1, adj[0]=4 -> 5.
    StudioBuilder b;
    const std::size_t bone_off = b.add_bone(
        -1, { 0, -1, -1, -1, -1, -1 },
        { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } );
    std::array<std::vector<std::int16_t>, 6> ch{}; // all channels absent
    const std::size_t anim_off = b.add_anim( ch );

    const auto &bytes = b.bytes();
    const BoneView bone{ bytes, bone_off };
    const AnimView anim{ bytes, anim_off };

    const float adj[1] = { 4.0f };
    Vec3 pos;
    xash::content::calc_bones( 0, 0.0f, bone, anim, adj, pos, nullptr );
    CHECK( pos.x == 5.0f ); // value[0] + fadj (default-bone branch, no scale)
}

// ---------------------------------------------------------------------------
// calc_bone_adj — controller -> adj table
// ---------------------------------------------------------------------------

static void test_calc_bone_adj()
{
    constexpr float k_pi_f = static_cast<float>( 3.14159265358979323846 );

    StudioBuilder b;
    // 4 controllers: X, XR, RLOOP|X, and a mouth slot (must be skipped).
    const std::size_t first = b.add_bonecontroller( 0, xash::content::k_studio_x, 0.0f, 1.0f, 0 );
    b.add_bonecontroller( 0, xash::content::k_studio_xr, 0.0f, 180.0f, 1 );
    b.add_bonecontroller( 0, xash::content::k_studio_rloop | xash::content::k_studio_x, 5.0f, 0.0f, 2 );
    b.add_bonecontroller( 0, xash::content::k_studio_x, 0.0f, 1.0f, xash::content::k_studio_mouth );
    b.header_i32( 148, 4 );                                  // numbonecontrollers
    b.header_i32( 152, static_cast<std::int32_t>( first ) ); // bonecontrollerindex

    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    const std::uint8_t pctl[5] = { 255, 255, 0, 0, 0 };
    float adj[32] = {};
    xash::content::calc_bone_adj( adj, pctl, hdr );

    CHECK( adj[0] == 1.0f );                          // X: value 255/255 -> 1
    CHECK( adj[1] == 180.0f * ( k_pi_f / 180.0f ) );  // XR: value 180 -> rad
    CHECK( adj[2] == 5.0f );                          // RLOOP|X: 0*(360/256)+5
    CHECK( adj[3] == 0.0f );                          // mouth slot skipped
}

// ---------------------------------------------------------------------------
// setup_bones — the driver (reduced-input exact + composition oracle)
// ---------------------------------------------------------------------------

using xash::content::BoneSetupInput;
using xash::content::BuiltinBoneSolver;
using xash::utilities::Matrix3x4;

// Build a single-bone, single-sequence, bind-pose (no anim) model with the given
// bone position/rotation baked into value[].
static StudioBuilder make_one_bone( std::int32_t parent, const std::array<float, 6> &value )
{
    StudioBuilder b;
    const std::size_t bone_off = b.add_bone( parent, { -1, -1, -1, -1, -1, -1 }, value,
                                             { 0, 0, 0, 0, 0, 0 } );
    const std::array<std::vector<std::int16_t>, 6> empty{};
    const std::size_t anim_off = b.add_anim_block( { empty } );
    const std::size_t seq_off = b.add_seqdesc( 1, 0, 0, 1, static_cast<std::int32_t>( anim_off ), 0 );
    b.header_i32( 140, 1 );                                        // numbones
    b.header_i32( 144, static_cast<std::int32_t>( bone_off ) );    // boneindex
    b.header_i32( 164, 1 );                                        // numseq
    b.header_i32( 168, static_cast<std::int32_t>( seq_off ) );     // seqindex
    return b;
}

static void test_setup_bones_reduced_pipeline()
{
    // identity entity + identity bone rotation, no anim -> world = origin + pos.
    StudioBuilder b = make_one_bone( -1, { 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f } );
    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    std::array<Matrix3x4, 1> out{};
    BoneSetupInput in;
    in.origin = { 10.0f, 20.0f, 30.0f };
    BuiltinBoneSolver solver;
    const int n = solver.setup_bones( hdr, in, out );

    CHECK( n == 1 );
    CHECK( out[0].m[0][0] == 1.0f && out[0].m[0][1] == 0.0f && out[0].m[0][2] == 0.0f && out[0].m[0][3] == 11.0f );
    CHECK( out[0].m[1][0] == 0.0f && out[0].m[1][1] == 1.0f && out[0].m[1][2] == 0.0f && out[0].m[1][3] == 22.0f );
    CHECK( out[0].m[2][0] == 0.0f && out[0].m[2][1] == 0.0f && out[0].m[2][2] == 1.0f && out[0].m[2][3] == 33.0f );
}

static void test_setup_bones_parent_chain()
{
    // two bones, child (1) parented to root (0). Identity rotations -> the child
    // world translation accumulates: origin + pos0 + pos1.
    StudioBuilder b;
    const std::size_t b0 = b.add_bone( -1, { -1, -1, -1, -1, -1, -1 }, { 1, 2, 3, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } );
    (void)b0;
    b.add_bone( 0, { -1, -1, -1, -1, -1, -1 }, { 4, 5, 6, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } );
    const std::array<std::vector<std::int16_t>, 6> empty{};
    const std::size_t anim_off = b.add_anim_block( { empty, empty } );
    const std::size_t seq_off = b.add_seqdesc( 1, 0, 0, 1, static_cast<std::int32_t>( anim_off ), 0 );
    b.header_i32( 140, 2 );
    b.header_i32( 144, static_cast<std::int32_t>( b0 ) );
    b.header_i32( 164, 1 );
    b.header_i32( 168, static_cast<std::int32_t>( seq_off ) );
    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    std::array<Matrix3x4, 2> out{};
    BoneSetupInput in;
    in.origin = { 10.0f, 20.0f, 30.0f };
    BuiltinBoneSolver solver;
    const int n = solver.setup_bones( hdr, in, out );

    CHECK( n == 2 );
    CHECK( out[0].m[0][3] == 11.0f && out[0].m[1][3] == 22.0f && out[0].m[2][3] == 33.0f );
    CHECK( out[1].m[0][3] == 15.0f && out[1].m[1][3] == 27.0f && out[1].m[2][3] == 39.0f ); // +{4,5,6}
}

static void test_setup_bones_sequence_clamp()
{
    // an out-of-range sequence clamps to 0 (same result as sequence 0).
    StudioBuilder b = make_one_bone( -1, { 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f } );
    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    std::array<Matrix3x4, 1> a{}, c{};
    BoneSetupInput in;
    in.origin = { 10.0f, 20.0f, 30.0f };
    BuiltinBoneSolver solver;
    in.sequence = 0;  (void)solver.setup_bones( hdr, in, a );
    in.sequence = 99; (void)solver.setup_bones( hdr, in, c );
    CHECK( mat_eq( a[0], c[0] ) );
}

static void test_setup_bones_rotation_oracle()
{
    // rotated entity + a non-identity bind-pose rotation. Verify the driver
    // composes exactly: world = entity_transform * from_origin_quat(q, pos),
    // with q/pos from the golden-verified primitives.
    StudioBuilder b = make_one_bone( -1, { 5.0f, 6.0f, 7.0f, 0.3f, -0.1f, 0.2f } );
    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    std::array<Matrix3x4, 1> out{};
    BoneSetupInput in;
    in.angles = { 10.0f, 20.0f, 30.0f };
    in.origin = { 1.0f, 2.0f, 3.0f };
    BuiltinBoneSolver solver;
    (void)solver.setup_bones( hdr, in, out );

    const Vec4 q = xash::utilities::angle_quaternion_studio( { 0.3f, -0.1f, 0.2f } );
    const Matrix3x4 bonematrix = xash::utilities::from_origin_quat( q, { 5.0f, 6.0f, 7.0f } );
    const Matrix3x4 entity = xash::utilities::create_from_entity( { 1.0f, 2.0f, 3.0f }, { 10.0f, 20.0f, 30.0f }, 1.0f );
    CHECK( mat_eq( out[0], xash::utilities::concat( entity, bonematrix ) ) );
}

static void test_setup_bones_numblends2_oracle()
{
    // 1 bone, 2 blends differing in X position; verify the blend composition
    // (slerp_bones by pblending[0]/255 then concat) against the public primitives.
    StudioBuilder b;
    const std::size_t bone_off = b.add_bone( -1, { -1, -1, -1, -1, -1, -1 },
                                             { 0, 0, 0, 0, 0, 0 }, { 1, 0, 0, 0, 0, 0 } );
    std::array<std::vector<std::int16_t>, 6> a0{}, a1{};
    a0[0] = { anim_num( 2, 2 ), 10, 20 }; // blend0 X pos
    a1[0] = { anim_num( 2, 2 ), 30, 40 }; // blend1 X pos
    const std::size_t anim_off = b.add_anim_block( { a0, a1 } );
    const std::size_t seq_off = b.add_seqdesc( 2, 0, 0, 2, static_cast<std::int32_t>( anim_off ), 0 );
    b.header_i32( 140, 1 );
    b.header_i32( 144, static_cast<std::int32_t>( bone_off ) );
    b.header_i32( 164, 1 );
    b.header_i32( 168, static_cast<std::int32_t>( seq_off ) );
    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    const std::uint8_t blend_bytes[1] = { 128 };
    std::array<Matrix3x4, 1> out{};
    BoneSetupInput in;
    in.blending = blend_bytes;
    BuiltinBoneSolver solver;
    (void)solver.setup_bones( hdr, in, out );

    // Oracle: replicate calc_rotations for each blend via the public calc_bones,
    // then slerp_bones + concat exactly as the driver does (frame 0, s 0).
    const BoneView bone = hdr.bone( 0 );
    const AnimView anim0{ bytes, anim_off };
    const AnimView anim1{ bytes, anim_off + xash::content::k_studio_anim_stride };
    Vec3 pos0, pos1;
    Vec4 q0, q1;
    xash::content::calc_bones( 0, 0.0f, bone, anim0, {}, pos0, &q0 );
    xash::content::calc_bones( 0, 0.0f, bone, anim1, {}, pos1, &q1 );
    xash::utilities::slerp_bones( { &q0, 1 }, { &pos0, 1 }, { &q1, 1 }, { &pos1, 1 }, 128.0f / 255.0f );
    const Matrix3x4 entity = xash::utilities::create_from_entity( {}, {}, 1.0f );
    const Matrix3x4 expect = xash::utilities::concat( entity, xash::utilities::from_origin_quat( q0, pos0 ) );
    CHECK( mat_eq( out[0], expect ) );
}

static void test_bone_world_position()
{
    // identity entity, bind pose -> bone 0 world origin = origin + pos, angles 0.
    StudioBuilder b = make_one_bone( -1, { 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f } );
    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    BoneSetupInput in;
    in.origin = { 10.0f, 20.0f, 30.0f };
    BuiltinBoneSolver solver;
    Vec3 origin{ -1, -1, -1 }, angles{ -1, -1, -1 };
    const bool ok = xash::content::bone_world_position( hdr, in, 0, solver, &origin, &angles );
    CHECK( ok );
    CHECK( origin.x == 11.0f && origin.y == 22.0f && origin.z == 33.0f );
    CHECK( angles.x == 0.0f && angles.y == 0.0f && angles.z == 0.0f );

    // out-of-range bone -> false, outs untouched.
    Vec3 o2{ 7, 7, 7 };
    CHECK( !xash::content::bone_world_position( hdr, in, 5, solver, &o2, nullptr ) );
    CHECK( o2.x == 7.0f );
}

static void test_attachment_world_position()
{
    // 1 bone + 1 attachment (bone 0, local org {1,0,0}); identity entity, bind
    // pose -> attachment world = (origin + pos) + org.
    StudioBuilder b;
    const std::size_t bone_off = b.add_bone( -1, { -1, -1, -1, -1, -1, -1 }, { 1, 2, 3, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } );
    const std::array<std::vector<std::int16_t>, 6> empty{};
    const std::size_t anim_off = b.add_anim_block( { empty } );
    const std::size_t seq_off = b.add_seqdesc( 1, 0, 0, 1, static_cast<std::int32_t>( anim_off ), 0 );
    const std::size_t att_off = b.add_attachment( 0, 1.0f, 0.0f, 0.0f );
    b.header_i32( 140, 1 );  b.header_i32( 144, static_cast<std::int32_t>( bone_off ) );
    b.header_i32( 164, 1 );  b.header_i32( 168, static_cast<std::int32_t>( seq_off ) );
    b.header_i32( 212, 1 );  b.header_i32( 216, static_cast<std::int32_t>( att_off ) ); // numattachments / index
    const auto &bytes = b.bytes();
    const StudioView hdr{ bytes };

    BoneSetupInput in;
    in.origin = { 10.0f, 20.0f, 30.0f };
    BuiltinBoneSolver solver;
    Vec3 origin{};
    const bool ok = xash::content::attachment_world_position( hdr, in, 0, solver, &origin, nullptr );
    CHECK( ok );
    CHECK( origin.x == 12.0f && origin.y == 22.0f && origin.z == 33.0f ); // {11,22,33}+{1,0,0}

    // a model with no attachments -> false.
    StudioBuilder nb = make_one_bone( -1, { 0, 0, 0, 0, 0, 0 } );
    const auto &nbytes = nb.bytes();
    const StudioView nhdr{ nbytes };
    Vec3 o2{ 5, 5, 5 };
    CHECK( !xash::content::attachment_world_position( nhdr, in, 0, solver, &o2, nullptr ) );
}

int main()
{
    RUN_TEST( test_calc_bones_rle_position );
    RUN_TEST( test_calc_bones_position_only_matches_full );
    RUN_TEST( test_calc_bones_rotation_slerp_oracle );
    RUN_TEST( test_calc_bones_controller_adj );
    RUN_TEST( test_calc_bone_adj );

    RUN_TEST( test_setup_bones_reduced_pipeline );
    RUN_TEST( test_setup_bones_parent_chain );
    RUN_TEST( test_setup_bones_sequence_clamp );
    RUN_TEST( test_setup_bones_rotation_oracle );
    RUN_TEST( test_setup_bones_numblends2_oracle );

    RUN_TEST( test_bone_world_position );
    RUN_TEST( test_attachment_world_position );

    std::printf( "test_studio_bones: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
