// xash3dpp — vendored ABI layout parity (Chunk 6 S4, Q-20)
// Compares every field offset and every struct size of the vendored
// xash::abi entvars_t / edict_t / globalvars_t against the ACTUAL legacy
// headers (engine/progdefs.h, engine/edict.h), included verbatim below
// with their const.h dependency guarded off.  Any layout drift — field
// order, type width, implicit padding — fails here before it can reach a
// game DLL.

#include <xash3dpp/abi/edict.hpp>

#include "../../test_helpers.hpp"

#include <cstddef>

// ---------------------------------------------------------------------------
// Legacy side: include the real headers in a sealed namespace.  The
// prerequisite typedefs normally supplied by const.h/xash3d_types.h are
// provided here; CONST_H is pre-defined so the const.h include resolves
// (common/ is on the include path) but contributes nothing.
// ---------------------------------------------------------------------------

namespace legacy {

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef int   string_t;
typedef unsigned char byte;
typedef int   qboolean;
typedef struct edict_s edict_t;
typedef struct link_s
{
    struct link_s *prev, *next;
} link_t;

#define CONST_H
#include <engine/progdefs.h>
#include <engine/edict.h>
#undef CONST_H

} // namespace legacy

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Field lists (transcribed from the legacy headers; the X-macro generates
// one offset comparison per field on both sides)
// ---------------------------------------------------------------------------

#define ENTVARS_FIELDS( X ) \
    X( classname ) X( globalname ) \
    X( origin ) X( oldorigin ) X( velocity ) X( basevelocity ) \
    X( clbasevelocity ) X( movedir ) \
    X( angles ) X( avelocity ) X( punchangle ) X( v_angle ) \
    X( endpos ) X( startpos ) X( impacttime ) X( starttime ) \
    X( fixangle ) X( idealpitch ) X( pitch_speed ) X( ideal_yaw ) X( yaw_speed ) \
    X( modelindex ) \
    X( model ) X( viewmodel ) X( weaponmodel ) \
    X( absmin ) X( absmax ) X( mins ) X( maxs ) X( size ) \
    X( ltime ) X( nextthink ) \
    X( movetype ) X( solid ) \
    X( skin ) X( body ) X( effects ) X( gravity ) X( friction ) \
    X( light_level ) \
    X( sequence ) X( gaitsequence ) X( frame ) X( animtime ) X( framerate ) \
    X( controller ) X( blending ) \
    X( scale ) X( rendermode ) X( renderamt ) X( rendercolor ) X( renderfx ) \
    X( health ) X( frags ) X( weapons ) X( takedamage ) \
    X( deadflag ) X( view_ofs ) \
    X( button ) X( impulse ) \
    X( chain ) X( dmg_inflictor ) X( enemy ) X( aiment ) X( owner ) X( groundentity ) \
    X( spawnflags ) X( flags ) \
    X( colormap ) X( team ) \
    X( max_health ) X( teleport_time ) X( armortype ) X( armorvalue ) \
    X( waterlevel ) X( watertype ) \
    X( target ) X( targetname ) X( netname ) X( message ) \
    X( dmg_take ) X( dmg_save ) X( dmg ) X( dmgtime ) \
    X( noise ) X( noise1 ) X( noise2 ) X( noise3 ) \
    X( speed ) X( air_finished ) X( pain_finished ) X( radsuit_finished ) \
    X( pContainingEntity ) \
    X( playerclass ) X( maxspeed ) \
    X( fov ) X( weaponanim ) \
    X( pushmsec ) \
    X( bInDuck ) X( flTimeStepSound ) X( flSwimTime ) X( flDuckTime ) \
    X( iStepLeft ) X( flFallVelocity ) \
    X( gamestate ) \
    X( oldbuttons ) \
    X( groupinfo ) \
    X( iuser1 ) X( iuser2 ) X( iuser3 ) X( iuser4 ) \
    X( fuser1 ) X( fuser2 ) X( fuser3 ) X( fuser4 ) \
    X( vuser1 ) X( vuser2 ) X( vuser3 ) X( vuser4 ) \
    X( euser1 ) X( euser2 ) X( euser3 ) X( euser4 )

#define GLOBALVARS_FIELDS( X ) \
    X( time ) X( frametime ) X( force_retouch ) \
    X( mapname ) X( startspot ) \
    X( deathmatch ) X( coop ) X( teamplay ) X( serverflags ) X( found_secrets ) \
    X( v_forward ) X( v_up ) X( v_right ) \
    X( trace_allsolid ) X( trace_startsolid ) X( trace_fraction ) \
    X( trace_endpos ) X( trace_plane_normal ) X( trace_plane_dist ) \
    X( trace_ent ) X( trace_inopen ) X( trace_inwater ) \
    X( trace_hitgroup ) X( trace_flags ) \
    X( changelevel ) X( cdAudioTrack ) X( maxClients ) X( maxEntities ) \
    X( pStringBase ) X( pSaveData ) X( vecLandmarkOffset )

#define EDICT_FIELDS( X ) \
    X( free ) X( serialnumber ) X( area ) X( headnode ) X( num_leafs ) \
    X( leafnums32 ) X( leafnums16 ) \
    X( freetime ) X( pvPrivateData ) X( v )

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static void test_entvars_layout()
{
    CHECK_EQ( sizeof( legacy::entvars_t ), sizeof( xash::abi::entvars_t ));
#define X( f ) CHECK_EQ( offsetof( legacy::entvars_t, f ), \
                         offsetof( xash::abi::entvars_t, f ));
    ENTVARS_FIELDS( X )
#undef X
}

static void test_globalvars_layout()
{
    CHECK_EQ( sizeof( legacy::globalvars_t ), sizeof( xash::abi::globalvars_t ));
#define X( f ) CHECK_EQ( offsetof( legacy::globalvars_t, f ), \
                         offsetof( xash::abi::globalvars_t, f ));
    GLOBALVARS_FIELDS( X )
#undef X
}

static void test_edict_layout()
{
    CHECK_EQ( sizeof( legacy::edict_t ), sizeof( xash::abi::edict_t ));
    CHECK_EQ( sizeof( legacy::link_t ), sizeof( xash::abi::link_t ));
#define X( f ) CHECK_EQ( offsetof( legacy::edict_t, f ), \
                         offsetof( xash::abi::edict_t, f ));
    EDICT_FIELDS( X )
#undef X
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_entvars_layout );
    RUN_TEST( test_globalvars_layout );
    RUN_TEST( test_edict_layout );

    std::printf( "edict_layout: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
