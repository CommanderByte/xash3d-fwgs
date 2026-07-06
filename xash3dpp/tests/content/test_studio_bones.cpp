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

int main()
{
    RUN_TEST( test_calc_bones_rle_position );
    RUN_TEST( test_calc_bones_position_only_matches_full );
    RUN_TEST( test_calc_bones_rotation_slerp_oracle );
    RUN_TEST( test_calc_bones_controller_adj );
    RUN_TEST( test_calc_bone_adj );

    std::printf( "test_studio_bones: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
