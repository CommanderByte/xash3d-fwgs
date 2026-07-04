// xash3dpp — hull point contents (Chunk 5, C8)
// Covers: the box-hull chain walk, the on-plane FRONT-child tie-break
// (strict < 0 — the asymmetry vs point_leaf's <= 0, companion test in
// pvs/test_pvs_queries.cpp), the missing-planes CONTENTS_NONE guard, and
// non-axial plane_diff.
// Legacy reference: pm_trace.c PM_HullPointContents (:113-137).

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/trace.hpp>

#include "../../test_helpers.hpp"

#include <array>

namespace ml = xash::map_loader;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

static void test_box_contents()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    CHECK_EQ( ml::hull_point_contents( hull, hull.firstclipnode, { 0, 0, 0 } ),
              ml::k_contents_solid );
    CHECK_EQ( ml::hull_point_contents( hull, hull.firstclipnode, { 20, 0, 0 } ),
              ml::k_contents_empty );
    CHECK_EQ( ml::hull_point_contents( hull, hull.firstclipnode, { 0, 0, -20 } ),
              ml::k_contents_empty );
    CHECK_EQ( ml::hull_point_contents( hull, hull.firstclipnode, { 15.9f, -15.9f, 15.9f } ),
              ml::k_contents_solid );
}

static void test_on_plane_tiebreak_front()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    // Exactly on the maxs.x plane: PlaneDiff == 0 → NOT < 0 → children[0]
    // (front) → CONTENTS_EMPTY.  The point_leaf walk would go BACK here.
    CHECK_EQ( ml::hull_point_contents( hull, hull.firstclipnode, { 16.0f, 0, 0 } ),
              ml::k_contents_empty );

    // Exactly on the mins.x plane: diff == 0 → front → continue INTO the box
    // (node 1's front child is node 2) → solid.
    CHECK_EQ( ml::hull_point_contents( hull, hull.firstclipnode, { -16.0f, 0, 0 } ),
              ml::k_contents_solid );
}

static void test_missing_planes_contents_none()
{
    ml::TraceHull empty{};
    CHECK_EQ( ml::hull_point_contents( empty, 0, { 0, 0, 0 } ), ml::k_contents_none );
}

static void test_nonaxial_plane()
{
    // Single node splitting on a non-axial plane (type 3 → dot-product
    // path): normal {0.6, 0.8, 0}, dist 8; front → EMPTY, back → WATER.
    const std::array<ml::ClipNode32, 1> nodes = { {
        { 0, { ml::k_contents_empty, ml::k_contents_water } },
    } };
    const std::array<ml::Plane, 1> planes = { {
        { { 0.6f, 0.8f, 0.0f }, 8.0f, 3, 0 },
    } };
    ml::TraceHull hull{ nodes, planes, 0, 0, {}, {} };

    CHECK_EQ( ml::hull_point_contents( hull, 0, { 20, 0, 0 } ),  // 0.6*20-8 = 4
              ml::k_contents_empty );
    CHECK_EQ( ml::hull_point_contents( hull, 0, { 0, 0, 0 } ),   // -8
              ml::k_contents_water );
}

int main()
{
    RUN_TEST( test_box_contents );
    RUN_TEST( test_on_plane_tiebreak_front );
    RUN_TEST( test_missing_planes_contents_none );
    RUN_TEST( test_nonaxial_plane );

    std::printf( "point_contents: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
