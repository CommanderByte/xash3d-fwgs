// xash3dpp — vendored pmove ABI layout parity (Chunk 6 pmove-bridge P1)
// Compares every field offset and every struct size of the vendored
// xash::abi pmplane_t / pmtrace_t / physent_t / playermove_t against the
// ACTUAL legacy header (pm_shared/pm_defs.h), included verbatim below with
// its prerequisite typedefs supplied locally.  physent_t and playermove_t
// embed pointers, so this parity holds on whichever pointer width the build
// targets (32- or 64-bit) — any drift in field order, type width, or
// implicit padding fails here before it can reach the game DLL's PM_Move.

#include <xash3dpp/abi/pm_defs.hpp>

#include "../../test_helpers.hpp"

#include <cstddef>

// ---------------------------------------------------------------------------
// Legacy side: pm_defs.h in a sealed namespace.  It only #includes pm_info.h
// (MAX_PHYSINFO_STRING) and uses vec3_t/qboolean/byte, the complete types
// usercmd_t + pmtrace_t (transcribed here from common/q_client.h +
// common/pmove.h so we avoid pulling xash3d_types.h's stdint/system headers
// into the namespace), plus opaque model_s/hull_s/msurface_s/movevars_s and
// the anonymous-typedef trace_t.  Layouts below are byte-for-byte the SDK's.
// ---------------------------------------------------------------------------

namespace legacy {

typedef float         vec_t;
typedef vec_t         vec3_t[3];
typedef int           qboolean;
typedef unsigned char byte;

struct model_s;
struct hull_s;
struct msurface_s;
struct movevars_s;
typedef struct pm_trace_opaque_s trace_t; // anon typedef in const.h; opaque here

// common/q_client.h :30-53 — usercmd_t (STATIC_CHECK_SIZEOF pins 52 bytes)
typedef struct usercmd_s
{
    short         lerp_msec;
    char          msec;
    unsigned char pad1;
    vec3_t        viewangles;
    float         forwardmove;
    float         sidemove;
    float         upmove;
    unsigned char lightlevel;
    unsigned char pad2;
    unsigned short buttons;
    unsigned char impulse;
    unsigned char weaponselect;
    unsigned char pad3[2];
    int           reserved[4];
} usercmd_t;

// common/pmove.h :26-45 — pmplane_t (16) + pmtrace_t (68)
typedef struct pmplane_s
{
    vec3_t normal;
    float  dist;
} pmplane_t;

typedef struct pmtrace_s
{
    qboolean  allsolid;
    qboolean  startsolid;
    qboolean  inopen, inwater;
    float     fraction;
    vec3_t    endpos;
    pmplane_t plane;
    int       ent;
    vec3_t    deltavelocity;
    int       hitgroup;
} pmtrace_t;

#include <pm_shared/pm_defs.h>

} // namespace legacy

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Field lists (transcribed from pm_shared/pm_defs.h; the X-macro generates
// one offset comparison per field on both sides — pointer members included,
// so pointer-slot offsets are validated on whichever width the build targets)
// ---------------------------------------------------------------------------

#define PHYSENT_FIELDS( X ) \
    X( name ) X( player ) X( origin ) X( model ) X( studiomodel ) \
    X( mins ) X( maxs ) X( info ) X( angles ) \
    X( solid ) X( skin ) X( rendermode ) \
    X( frame ) X( sequence ) X( controller ) X( blending ) \
    X( movetype ) X( takedamage ) X( blooddecal ) X( team ) X( classnumber ) \
    X( iuser1 ) X( iuser2 ) X( iuser3 ) X( iuser4 ) \
    X( fuser1 ) X( fuser2 ) X( fuser3 ) X( fuser4 ) \
    X( vuser1 ) X( vuser2 ) X( vuser3 ) X( vuser4 )

#define PMTRACE_FIELDS( X ) \
    X( allsolid ) X( startsolid ) X( inopen ) X( inwater ) \
    X( fraction ) X( endpos ) X( plane ) X( ent ) \
    X( deltavelocity ) X( hitgroup )

#define PLAYERMOVE_FIELDS( X ) \
    X( player_index ) X( server ) X( multiplayer ) X( time ) X( frametime ) \
    X( forward ) X( right ) X( up ) \
    X( origin ) X( angles ) X( oldangles ) X( velocity ) X( movedir ) \
    X( basevelocity ) \
    X( view_ofs ) X( flDuckTime ) X( bInDuck ) \
    X( flTimeStepSound ) X( iStepLeft ) \
    X( flFallVelocity ) X( punchangle ) \
    X( flSwimTime ) X( flNextPrimaryAttack ) \
    X( effects ) X( flags ) X( usehull ) X( gravity ) X( friction ) \
    X( oldbuttons ) X( waterjumptime ) X( dead ) X( deadflag ) \
    X( spectator ) X( movetype ) \
    X( onground ) X( waterlevel ) X( watertype ) X( oldwaterlevel ) \
    X( sztexturename ) X( chtexturetype ) \
    X( maxspeed ) X( clientmaxspeed ) \
    X( iuser1 ) X( iuser2 ) X( iuser3 ) X( iuser4 ) \
    X( fuser1 ) X( fuser2 ) X( fuser3 ) X( fuser4 ) \
    X( vuser1 ) X( vuser2 ) X( vuser3 ) X( vuser4 ) \
    X( numphysent ) X( physents ) \
    X( nummoveent ) X( moveents ) \
    X( numvisent ) X( visents ) \
    X( cmd ) \
    X( numtouch ) X( touchindex ) \
    X( physinfo ) \
    X( movevars ) X( player_mins ) X( player_maxs ) \
    X( PM_Info_ValueForKey ) X( PM_Particle ) X( PM_TestPlayerPosition ) \
    X( Con_NPrintf ) X( Con_DPrintf ) X( Con_Printf ) X( Sys_FloatTime ) \
    X( PM_StuckTouch ) X( PM_PointContents ) X( PM_TruePointContents ) \
    X( PM_HullPointContents ) X( PM_PlayerTrace ) X( PM_TraceLine ) \
    X( RandomLong ) X( RandomFloat ) X( PM_GetModelType ) X( PM_GetModelBounds ) \
    X( PM_HullForBsp ) X( PM_TraceModel ) \
    X( COM_FileSize ) X( COM_LoadFile ) X( COM_FreeFile ) X( memfgets ) \
    X( runfuncs ) X( PM_PlaySound ) X( PM_TraceTexture ) \
    X( PM_PlaybackEventFull ) X( PM_PlayerTraceEx ) X( PM_TestPlayerPositionEx ) \
    X( PM_TraceLineEx ) X( PM_TraceSurface )

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static void test_pmplane_layout()
{
    CHECK_EQ( sizeof( legacy::pmplane_t ), sizeof( xash::abi::pmplane_t ));
    CHECK_EQ( sizeof( xash::abi::pmplane_t ), static_cast<std::size_t>( 16 ));
}

// Compare BOTH the offset and the member size of each field.  offsetof alone
// misses a same-width type swap that leaves following fields in place (the
// abi-watchdog P1 caveat); adding sizeof catches pointer-width and array-
// dimension drift at an unchanged offset.  A pure int<->float swap at equal
// offset AND equal size is the residual gap, covered by both sides being
// transcribed from the same legacy header and manual watchdog review.
#define CHECK_FIELD( T, f ) \
    do { \
        CHECK_EQ( offsetof( legacy::T, f ), offsetof( xash::abi::T, f )); \
        CHECK_EQ( sizeof( legacy::T::f ), sizeof( xash::abi::T::f )); \
    } while ( 0 )

static void test_pmtrace_layout()
{
    CHECK_EQ( sizeof( legacy::pmtrace_t ), sizeof( xash::abi::pmtrace_t ));
    CHECK_EQ( sizeof( xash::abi::pmtrace_t ), static_cast<std::size_t>( 68 ));
#define X( f ) CHECK_FIELD( pmtrace_t, f );
    PMTRACE_FIELDS( X )
#undef X
}

static void test_physent_layout()
{
    CHECK_EQ( sizeof( legacy::physent_t ), sizeof( xash::abi::physent_t ));
#define X( f ) CHECK_FIELD( physent_t, f );
    PHYSENT_FIELDS( X )
#undef X
}

static void test_playermove_layout()
{
    CHECK_EQ( sizeof( legacy::playermove_t ), sizeof( xash::abi::playermove_t ));
#define X( f ) CHECK_FIELD( playermove_t, f );
    PLAYERMOVE_FIELDS( X )
#undef X
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_pmplane_layout );
    RUN_TEST( test_pmtrace_layout );
    RUN_TEST( test_physent_layout );
    RUN_TEST( test_playermove_layout );

    std::printf( "pmove_layout: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
