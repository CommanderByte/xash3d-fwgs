// xash3dpp — trace kernel golden vectors (Chunk 5, C8 — the Q-18 gate)
// Every expected float below is HAND-DERIVED (derivations inline) and was
// cross-checked bit-for-bit against the legacy PM_RecursiveHullCheck /
// PM_HullPointContents compiled verbatim in a throwaway harness (the
// harness is not committed; only the verified literals are).
// Where ULP-exactness is the contract the comparison is on bit patterns.
// Legacy reference: pm_trace.c:200-323, pm_local.h PM_InitPMTrace.

#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/trace.hpp>

#include "../../test_helpers.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace ml = xash::map_loader;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

static std::uint32_t bits( float f ) { return std::bit_cast<std::uint32_t>( f ); }

// ---------------------------------------------------------------------------
// straight hit on an axial box face
// ---------------------------------------------------------------------------

static void test_axial_box_hit()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    // Ray {32,0,0} → {0,0,0} crosses the maxs.x plane (node 0, dist 16):
    //   t1 = 32-16 = 16, t2 = 0-16 = -16, side = 0 (t1 >= 0)
    //   frac = (t1 - 1/32) / (t1 - t2) = 15.96875 / 32 = 511/1024
    //        = 0.4990234375 (exactly representable)
    //   mid.x = 32 + frac*(0-32) = 32 - 15.96875 = 16.03125 (exact)
    // Near side (children[0] = EMPTY) opens; far side (node 1 subtree) is
    // solid at mid → impact on plane {1,0,0}, dist 16.  The root re-check at
    // mid is EMPTY (mid is epsilon-outside), so the backup loop is idle.
    const auto tr = ml::trace_hull( hull, { 32, 0, 0 }, { 0, 0, 0 } );

    CHECK( !tr.allsolid );
    CHECK( !tr.startsolid );
    CHECK( tr.inopen );
    CHECK( !tr.inwater );
    CHECK_EQ( bits( tr.fraction ), bits( 0.4990234375f ));
    CHECK_EQ( bits( tr.endpos.x ), bits( 16.03125f ));
    CHECK_EQ( bits( tr.endpos.y ), bits( 0.0f ));
    CHECK( tr.plane.normal.x == 1.0f && tr.plane.normal.y == 0.0f );
    CHECK_EQ( bits( tr.plane.dist ), bits( 16.0f ));
}

static void test_axial_box_hit_from_negative_side()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    // Ray {-32,0,0} → {0,0,0}: at node 0 (x=16) both points are behind →
    // tail-loop to node 1 (x=-16): t1 = -32+16 = -16, t2 = 0+16 = 16,
    // side = 1 (t1 < 0):
    //   frac = (t1 + 1/32) / (t1 - t2) = -15.96875 / -32 = 0.4990234375
    //   mid.x = -32 + frac*32 = -16.03125
    // Impact on the BACK of plane 1 → normal negated {-1,0,0}, dist = -(-16) = 16.
    const auto tr = ml::trace_hull( hull, { -32, 0, 0 }, { 0, 0, 0 } );

    CHECK( !tr.allsolid );
    CHECK( tr.inopen );
    CHECK_EQ( bits( tr.fraction ), bits( 0.4990234375f ));
    CHECK_EQ( bits( tr.endpos.x ), bits( -16.03125f ));
    CHECK( tr.plane.normal.x == -1.0f );
    CHECK_EQ( bits( tr.plane.dist ), bits( 16.0f ));
}

// ---------------------------------------------------------------------------
// miss / fully-open ray
// ---------------------------------------------------------------------------

static void test_miss_keeps_fraction_one()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    // y = 20 passes beside the box: crosses the x-plane band but the far
    // subtree resolves to EMPTY at every leaf.
    const auto tr = ml::trace_hull( hull, { 32, 20, 0 }, { 0, 20, 0 } );

    CHECK( !tr.allsolid );
    CHECK( !tr.startsolid );
    CHECK( tr.inopen );
    CHECK_EQ( bits( tr.fraction ), bits( 1.0f ));
    CHECK( tr.endpos.x == 0.0f && tr.endpos.y == 20.0f ); // stays at init end
}

// ---------------------------------------------------------------------------
// startsolid / allsolid
// ---------------------------------------------------------------------------

static void test_fully_inside_allsolid()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    auto tr = ml::trace_hull( hull, { 0, 0, 0 }, { 8, 0, 0 } );

    CHECK( tr.allsolid );   // never cleared: every leaf on the ray is solid
    CHECK( tr.startsolid );
    CHECK( !tr.inopen );
    CHECK_EQ( bits( tr.fraction ), bits( 1.0f )); // kernel never wrote it

    // Wrapper semantics: allsolid → startsolid → fraction 0.
    ml::finalize_trace( tr, { 0, 0, 0 }, { 8, 0, 0 } );
    CHECK( tr.startsolid );
    CHECK_EQ( bits( tr.fraction ), bits( 0.0f ));
    CHECK( tr.endpos.x == 8.0f ); // untouched by finalize when startsolid
}

static void test_start_solid_exit_to_open()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    // Ray {8,0,0} → {40,0,0}: near side (inside) is solid → startsolid;
    // the far side at the crosspoint is EMPTY → "go past" → ray ends open.
    // No impact is recorded: fraction stays 1, allsolid was cleared by the
    // open far segment.
    auto tr = ml::trace_hull( hull, { 8, 0, 0 }, { 40, 0, 0 } );

    CHECK( !tr.allsolid );
    CHECK( tr.startsolid );
    CHECK( tr.inopen );
    CHECK_EQ( bits( tr.fraction ), bits( 1.0f ));

    ml::finalize_trace( tr, { 8, 0, 0 }, { 40, 0, 0 } );
    CHECK_EQ( bits( tr.fraction ), bits( 0.0f )); // startsolid → 0
}

// ---------------------------------------------------------------------------
// water transit
// ---------------------------------------------------------------------------

static void test_water_pocket_inopen_inwater()
{
    // Plane x=0: front EMPTY, back WATER — a transit records both
    // environment bits and no impact.  (Two nodes: a single-node hull has
    // first == last, which trips the legacy "empty hull?" guard.)
    const std::array<ml::ClipNode32, 2> nodes2 = { {
        { 0, { ml::k_contents_empty, 1 } },
        { 1, { ml::k_contents_water, ml::k_contents_water } },
    } };
    const std::array<ml::Plane, 2> planes2 = { {
        { { 1, 0, 0 }, 0.0f, 0, 0 },
        { { 0, 1, 0 }, 0.0f, 1, 0 },
    } };
    const ml::TraceHull hull2{ nodes2, planes2, 0, 1, {}, {} };

    const auto tr = ml::trace_hull( hull2, { 8, 4, 0 }, { -8, 4, 0 } );

    CHECK( !tr.allsolid );
    CHECK( !tr.startsolid );
    CHECK( tr.inopen );
    CHECK( tr.inwater );
    CHECK_EQ( bits( tr.fraction ), bits( 1.0f )); // water is not an impact
}

// ---------------------------------------------------------------------------
// degenerate hull / bad node / allsolid impact suppression
// ---------------------------------------------------------------------------

static void test_degenerate_hull_is_open()
{
    const std::array<ml::ClipNode32, 1> nodes = { {
        { 0, { ml::k_contents_solid, ml::k_contents_solid } },
    } };
    const std::array<ml::Plane, 1> planes = { {
        { { 1, 0, 0 }, 0.0f, 0, 0 },
    } };
    // firstclipnode >= lastclipnode → "empty hull?" → open, allsolid cleared.
    const ml::TraceHull hull{ nodes, planes, 0, 0, {}, {} };

    const auto tr = ml::trace_hull( hull, { 8, 0, 0 }, { -8, 0, 0 } );
    CHECK( !tr.allsolid );
    CHECK( tr.inopen );
    CHECK_EQ( bits( tr.fraction ), bits( 1.0f ));
}

static void test_bad_node_number_aborts()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    ml::TraceResult tr{};
    tr.endpos = { -8, 0, 0 };
    // Legacy Host_Error path: we log and abort the trace (returns false).
    CHECK( !ml::recursive_hull_check( hull, 99, 0.0f, 1.0f,
                                      { 8, 0, 0 }, { -8, 0, 0 }, tr ));
    CHECK_EQ( bits( tr.fraction ), bits( 1.0f )); // untouched
}

static void test_allsolid_suppresses_impact_plane()
{
    // Consistent DAG hull: node 0 splits on x=0 with BOTH children → node 1,
    // node 1 (same plane values) → front SOLID, back EMPTY.  A ray from the
    // solid side crossing out: near segment is all solid (allsolid stays
    // true at the impact check) → "never got out of the solid area" →
    // return false WITHOUT recording a plane.
    const std::array<ml::ClipNode32, 2> nodes = { {
        { 0, { 1, 1 } },
        { 1, { ml::k_contents_solid, ml::k_contents_empty } },
    } };
    const std::array<ml::Plane, 2> planes = { {
        { { 1, 0, 0 }, 0.0f, 0, 0 },
        { { 1, 0, 0 }, 0.0f, 0, 0 },
    } };
    const ml::TraceHull hull{ nodes, planes, 0, 1, {}, {} };

    const auto tr = ml::trace_hull( hull, { 8, 0, 0 }, { -8, 0, 0 } );

    CHECK( tr.allsolid );
    CHECK( tr.startsolid );
    CHECK_EQ( bits( tr.fraction ), bits( 1.0f ));   // no impact written
    CHECK( tr.plane.normal.x == 0.0f );             // plane never recorded
}

// ---------------------------------------------------------------------------
// non-axial plane (dot-product path)
// ---------------------------------------------------------------------------

static void test_nonaxial_wedge_hit()
{
    // Wedge face: plane normal {0.6, 0.8, 0} (type 3 → dot-product path),
    // dist 8; front EMPTY, back chains to a solid closer.
    // Ray {20,0,0} → {0,0,0}: note 0.6f is not exactly 0.6, so
    //   t1 = 0.6f*20 + 0.8f*0 + 0*0 - 8   (≈ 4.0000005, NOT exactly 4)
    //   t2 = -8 exactly; side = 0; frac = (t1 - 1/32) / (t1 - t2)
    // The expectation below recomputes t1 with the same single-precision
    // expression plane_diff uses; the resulting bit patterns were
    // additionally cross-checked against the legacy kernel in the harness.
    const std::array<ml::ClipNode32, 2> nodes = { {
        { 0, { ml::k_contents_empty, 1 } },
        { 1, { ml::k_contents_solid, ml::k_contents_solid } },
    } };
    const std::array<ml::Plane, 2> planes = { {
        { { 0.6f, 0.8f, 0.0f }, 8.0f, 3, 0 },
        { { 0.0f, 0.0f, 1.0f }, -64.0f, 2, 0 },
    } };
    const ml::TraceHull hull{ nodes, planes, 0, 1, {}, {} };

    const auto tr = ml::trace_hull( hull, { 20, 0, 0 }, { 0, 0, 0 } );

    CHECK( !tr.allsolid );
    CHECK( tr.inopen );
    CHECK( tr.plane.normal.x == 0.6f && tr.plane.normal.y == 0.8f );
    CHECK_EQ( bits( tr.plane.dist ), bits( 8.0f ));
    const float t1 = 0.6f * 20.0f + 0.8f * 0.0f + 0.0f * 0.0f - 8.0f;
    const float t2 = 0.6f * 0.0f + 0.8f * 0.0f + 0.0f * 0.0f - 8.0f;
    const float expect_frac = ( t1 - ml::k_dist_epsilon ) / ( t1 - t2 );
    CHECK_EQ( bits( tr.fraction ), bits( expect_frac ));
    const float expect_mid_x = 20.0f + expect_frac * ( 0.0f - 20.0f );
    CHECK_EQ( bits( tr.endpos.x ), bits( expect_mid_x ));
}

// ---------------------------------------------------------------------------
// finalize_trace world-frame recompute
// ---------------------------------------------------------------------------

static void test_finalize_world_frame()
{
    ml::BoxHull box;
    const auto &hull = box.set_bounds( { -16, -16, -16 }, { 16, 16, 16 } );

    // Trace in a local frame offset by {100,0,0}: world ray {132,0,0} →
    // {100,0,0} against a hull at origin (local {32,0,0} → {0,0,0}).
    auto tr = ml::trace_hull( hull, { 32, 0, 0 }, { 0, 0, 0 } );
    REQUIRE( bits( tr.fraction ) == bits( 0.4990234375f ));

    ml::finalize_trace( tr, { 132, 0, 0 }, { 100, 0, 0 } );

    // endpos = 132 + frac*(100-132) = 132 - 15.96875 = 116.03125 (exact)
    CHECK_EQ( bits( tr.endpos.x ), bits( 116.03125f ));
    // plane.dist recomputed = dot(endpos, {1,0,0}) = 116.03125 (NOT 16!)
    CHECK_EQ( bits( tr.plane.dist ), bits( 116.03125f ));
}

// ---------------------------------------------------------------------------
// BoxHull::set_planes — the studio hitbox hull (six oriented planes over the box
// clipnode chain). Fed the axial box planes it must trace bit-identically to
// set_bounds: the non-axial dot path yields the same values for unit-axis
// normals (dot(p,{1,0,0}) == p.x exactly).
// ---------------------------------------------------------------------------

static void test_set_planes_studio_hull()
{
    ml::BoxHull box;
    const ml::TracePlane planes[6] = {
        { { 1, 0, 0 },  16.0f }, { { 1, 0, 0 }, -16.0f },
        { { 0, 1, 0 },  16.0f }, { { 0, 1, 0 }, -16.0f },
        { { 0, 0, 1 },  16.0f }, { { 0, 0, 1 }, -16.0f },
    };
    const auto &hull = box.set_planes( planes );

    const auto tr = ml::trace_hull( hull, { 32, 0, 0 }, { 0, 0, 0 } );
    CHECK( !tr.allsolid && !tr.startsolid && tr.inopen );
    CHECK_EQ( bits( tr.fraction ), bits( 0.4990234375f ));
    CHECK_EQ( bits( tr.endpos.x ), bits( 16.03125f ));
    CHECK( tr.plane.normal.x == 1.0f );
    CHECK_EQ( bits( tr.plane.dist ), bits( 16.0f ));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_axial_box_hit );
    RUN_TEST( test_axial_box_hit_from_negative_side );
    RUN_TEST( test_miss_keeps_fraction_one );
    RUN_TEST( test_fully_inside_allsolid );
    RUN_TEST( test_start_solid_exit_to_open );
    RUN_TEST( test_water_pocket_inopen_inwater );
    RUN_TEST( test_degenerate_hull_is_open );
    RUN_TEST( test_bad_node_number_aborts );
    RUN_TEST( test_allsolid_suppresses_impact_plane );
    RUN_TEST( test_nonaxial_wedge_hit );
    RUN_TEST( test_finalize_world_frame );
    RUN_TEST( test_set_planes_studio_hull );

    std::printf( "hull_trace: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
