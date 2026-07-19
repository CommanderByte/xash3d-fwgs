// xash3dpp — studio ABI layout tripwire (Chunk 7 deferred item; closed by
// the 2026-07 consolidation audit, D5/abi-watchdog finding).
//
// Includes the ACTUAL legacy engine/studio.h in a sealed namespace and pins
// the hand-transcribed stride/offset constants content relies on
// (studio.hpp k_studio_*_stride; the file-local kOff* table in
// src/content/model/studio.cpp) against the real struct layout. Any drift —
// field order, type width, implicit padding — fails at COMPILE time here
// before it can corrupt a bone solve or hitbox hull.
//
// Same include dance as tests/server/abi/test_edict_layout.cpp:
// xash3d_types.h is guarded off and its prerequisites supplied inline.

#include <xash3dpp/content/studio.hpp>

#include "../test_helpers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace legacy {

typedef std::uint8_t byte;
typedef float  vec_t;
typedef vec_t  vec2_t[2];
typedef vec_t  vec3_t[3];
typedef vec_t  vec4_t[4];
typedef vec_t  quat_t[4];

#define XASH_TYPES_H
#include <engine/studio.h>

} // namespace legacy

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Strides (content/studio.hpp) vs the real legacy structs.
// ---------------------------------------------------------------------------

static_assert( sizeof( legacy::studiohdr_t )             == xash::content::k_studio_header_size,
               "studiohdr_t size drifted" );
static_assert( sizeof( legacy::mstudiobone_t )           == xash::content::k_studio_bone_stride,
               "mstudiobone_t stride drifted" );
static_assert( sizeof( legacy::mstudiobonecontroller_t ) == xash::content::k_studio_bonectrl_stride,
               "mstudiobonecontroller_t stride drifted" );
static_assert( sizeof( legacy::mstudioanim_t )           == xash::content::k_studio_anim_stride,
               "mstudioanim_t stride drifted" );
static_assert( sizeof( legacy::mstudioseqdesc_t )        == xash::content::k_studio_seqdesc_stride,
               "mstudioseqdesc_t stride drifted" );
static_assert( sizeof( legacy::mstudioattachment_t )     == xash::content::k_studio_attachment_stride,
               "mstudioattachment_t stride drifted" );
static_assert( sizeof( legacy::mstudiobbox_t )           == xash::content::k_studio_hitbox_stride,
               "mstudiobbox_t stride drifted" );

// ---------------------------------------------------------------------------
// studiohdr_t field offsets. The expected values are an INDEPENDENT
// transcription that must equal both the legacy offsetof AND the file-local
// kOff* constants in src/content/model/studio.cpp (change either → change
// both — that is the point of the tripwire).
// ---------------------------------------------------------------------------

#define HDR_OFF( field, expect ) \
    static_assert( offsetof( legacy::studiohdr_t, field ) == ( expect ), \
                   "studiohdr_t." #field " offset drifted" )

HDR_OFF( ident,               0 );
HDR_OFF( version,             4 );
HDR_OFF( name,                8 );
HDR_OFF( length,              72 );
HDR_OFF( eyeposition,         76 );
HDR_OFF( min,                 88 );
HDR_OFF( max,                 100 );
HDR_OFF( bbmin,               112 );
HDR_OFF( bbmax,               124 );
HDR_OFF( flags,               136 );
HDR_OFF( numbones,            140 );
HDR_OFF( boneindex,           144 );
HDR_OFF( numbonecontrollers,  148 );
HDR_OFF( bonecontrollerindex, 152 );
HDR_OFF( numhitboxes,         156 );
HDR_OFF( hitboxindex,         160 );
HDR_OFF( numseq,              164 );
HDR_OFF( seqindex,            168 );
HDR_OFF( numseqgroups,        172 );
HDR_OFF( seqgroupindex,       176 );
HDR_OFF( numtextures,         180 );
HDR_OFF( numbodyparts,        204 );
HDR_OFF( bodypartindex,       208 );
HDR_OFF( numattachments,      212 );
HDR_OFF( attachmentindex,     216 );
HDR_OFF( transitionindex,     240 );

#undef HDR_OFF

// ---------------------------------------------------------------------------
// Sub-struct offsets the bone solver / hitbox-hull code depends on.
// ---------------------------------------------------------------------------

static_assert( offsetof( legacy::mstudiobone_t, parent )         == 32 );
static_assert( offsetof( legacy::mstudiobone_t, bonecontroller ) == 40 );
static_assert( offsetof( legacy::mstudiobone_t, value )          == 64 );
static_assert( offsetof( legacy::mstudiobone_t, scale )          == 88 );
static_assert( offsetof( legacy::mstudiobbox_t, group )          == 4 );
static_assert( offsetof( legacy::mstudiobbox_t, bbmin )          == 8 );
static_assert( offsetof( legacy::mstudiobbox_t, bbmax )          == 20 );
static_assert( offsetof( legacy::mstudioattachment_t, bone )     == 36 );
static_assert( offsetof( legacy::mstudioattachment_t, org )      == 40 );
// SV_StudioPlayerBlend inputs (SeqDescView::blend_start0/blend_end0).
static_assert( offsetof( legacy::mstudioseqdesc_t, blendstart )  == 136 );
static_assert( offsetof( legacy::mstudioseqdesc_t, blendend )    == 144 );
static_assert( offsetof( legacy::mstudioseqdesc_t, numblends )   == 120 );

static void test_layout_pins()
{
    // Load-bearing pins re-checked at runtime so ctest reports the tripwire.
    CHECK( sizeof( legacy::studiohdr_t )   == xash::content::k_studio_header_size );
    CHECK( sizeof( legacy::mstudiobone_t ) == xash::content::k_studio_bone_stride );
    CHECK( sizeof( legacy::mstudiobbox_t ) == xash::content::k_studio_hitbox_stride );
    CHECK( offsetof( legacy::studiohdr_t, boneindex )   == 144u );
    CHECK( offsetof( legacy::studiohdr_t, hitboxindex ) == 160u );
    CHECK( STUDIO_VERSION == 10 ); // macro from the legacy header (not namespaced)
}

int main()
{
    RUN_TEST( test_layout_pins );
    std::printf( "studio_layout: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
