#pragma once
// xash3dpp — vendored frozen SDK structs: pmplane_t, pmtrace_t, physent_t,
// playermove_t
// Legacy reference: common/pmove.h (pmplane_t :26-30, pmtrace_t :34-45 —
// both pointer-free, legacy pins 16 / 68 bytes via STATIC_CHECK_SIZEOF on
// 32- and 64-bit), pm_shared/pm_defs.h (physent_t :37-77, playermove_t
// :79-215), pm_shared/pm_info.h (MAX_PHYSINFO_STRING).
//
// Byte-exact mirror of the pmove structures the game DLL reads and writes
// through raw pointers: the engine builds a playermove_t, hands it to the
// DLL's PM_Move export, and reads back the mutated state.  physent_t and
// playermove_t embed real pointers (model_s*, the ~30 host callback function
// pointers, movevars_t*), so their size is pointer-width dependent — unlike
// the pointer-free pmplane_t / pmtrace_t.  Field order, types, and implicit
// padding must never change.  tests/server/abi/test_pmove_layout.cpp pins
// every field offset against the actual legacy headers on both widths.
//
// Legacy type names are retained verbatim (ABI naming exemption — see
// .github/instructions/xash3dpp.instructions.md, Naming Conventions).

#include <xash3dpp/abi/abi_types.hpp>
#include <xash3dpp/abi/pm_movevars.hpp> // movevars_t (playermove_t::movevars)
#include <xash3dpp/abi/usercmd.hpp>     // usercmd_t (playermove_t::cmd)

#include <cstddef>
#include <type_traits>

// @annotation-exempt: abi-pod — every struct below is a byte-exact mirror of a
// frozen legacy pmove POD (pmplane_t / pmtrace_t / physent_t / playermove_t).
// physent_t and playermove_t embed raw ABI pointers (model_s*, movevars_t*) and
// playermove_t's ~30 host-callback slots are a frozen function-pointer table
// (fnptr-table); their ownership and calling contract are the engine/bridge's,
// not this header's.  The QN annotation matrix does not apply to these vendored
// PODs, and @thread-safety is a caller/engine contract (decisions-style QN).
namespace xash::abi {

// legacy: pm_shared/pm_defs.h — frozen array dimensions
inline constexpr int k_max_physents  = 600; // room for all world entities
inline constexpr int k_max_moveents  = 64;
inline constexpr int k_max_clip_planes = 5;

// legacy: pm_shared/pm_defs.h :23-32 — PM_PlayerTrace / PM_TraceLine flags.
// The DLL passes these to the trace callbacks; the trace family branches on
// them (PM_WORLD_ONLY stops after physents[0], the *_IGNORE flags skip
// entity classes).
inline constexpr int k_pm_normal        = 0x00000000;
inline constexpr int k_pm_studio_ignore = 0x00000001; // skip studio models
inline constexpr int k_pm_studio_box    = 0x00000002; // box-trace non-complex studio
inline constexpr int k_pm_glass_ignore  = 0x00000004; // skip non-normal rendermode
inline constexpr int k_pm_world_only    = 0x00000008; // trace against the world only
inline constexpr int k_pm_custom_ignore = 0x00000010; // skip SOLID_CUSTOM
// PM_TraceLine `flags` selector: physents vs any-visible ent list.
inline constexpr int k_pm_traceline_physentsonly = 0;
inline constexpr int k_pm_traceline_anyvisible   = 1;

// Opaque engine types referenced only through pointers in the host-callback
// function-pointer table below.  They never appear by value, so an incomplete
// declaration is ABI-sufficient (a pointer slot is a pointer slot); the pmove
// bridge that populates these slots supplies the concrete engine-side types.
// trace_t is a legacy *anonymous-struct* typedef (common/const.h) with no tag,
// so it gets its own opaque tag here rather than a `struct trace_s` fwd-decl.
struct model_s;
struct hull_s;
struct msurface_s;
struct trace_t;

// legacy: common/pmove.h :26-30 — surface plane at a pmove impact
struct pmplane_t
{
    vec3_t normal;
    float  dist;
};

static_assert( sizeof( pmplane_t ) == 16 );

// legacy: common/pmove.h :34-45 — pmove trace result (int `ent`, not edict*)
struct pmtrace_t
{
    qboolean  allsolid;   // if true, plane is not valid
    qboolean  startsolid; // if true, the initial point was in a solid area
    qboolean  inopen, inwater;
    float     fraction;   // time completed, 1.0 = didn't hit anything
    vec3_t    endpos;     // final position
    pmplane_t plane;      // surface normal at impact
    int       ent;        // entity the surface is on
    vec3_t    deltavelocity;
    int       hitgroup;
};

static_assert( sizeof( pmtrace_t ) == 68 );

// legacy: pm_shared/pm_defs.h :37-77 — one entity the mover clips against
struct physent_t
{
    char     name[32];   // Name of model, or "player" or "world".
    int      player;
    vec3_t   origin;     // Model's origin in world coordinates.
    model_s *model;      // only for bsp models @annotation-exempt: abi-pod
    model_s *studiomodel; // SOLID_BBOX, but studio clip intersections. @annotation-exempt: abi-pod
    vec3_t   mins, maxs; // only for non-bsp models
    int      info;       // index into edicts or cl_entities
    vec3_t   angles;     // rotated entities need this for hull testing

    int      solid;      // SOLID_NOT WATER brushes for triggers/func_door
    int      skin;       // BSP Contents for such brushes
    int      rendermode; // So we can ignore glass

    // Complex collision detection.
    float    frame;
    int      sequence;
    byte     controller[4];
    byte     blending[2];

    int      movetype;
    int      takedamage;
    int      blooddecal;
    int      team;
    int      classnumber;

    // For mods
    int      iuser1;
    int      iuser2;
    int      iuser3;
    int      iuser4;
    float    fuser1;     // also pev->scale when "sv_allow_studio_scaling" 1
    float    fuser2;
    float    fuser3;
    float    fuser4;
    vec3_t   vuser1;
    vec3_t   vuser2;
    vec3_t   vuser3;
    vec3_t   vuser4;
};

// legacy: pm_shared/pm_defs.h :79-215 — the whole pmove working set
struct playermove_t
{
    int      player_index; // So we don't PM_CheckStuck-nudge too quickly.
    qboolean server;       // are we running physics code on server side?

    qboolean multiplayer;  // 1 == multiplayer server
    float    time;         // realtime on host, for reckoning duck timing
    float    frametime;    // Duration of this frame

    vec3_t   forward, right, up; // Vectors for angles

    // player state
    vec3_t   origin;       // Movement origin.
    vec3_t   angles;       // Movement view angles.
    vec3_t   oldangles;    // Angles before movement view angles.
    vec3_t   velocity;     // Current movement direction.
    vec3_t   movedir;      // waterjump forced forward velocity
    vec3_t   basevelocity; // Velocity of the conveyor we stand on.

    // For ducking/dead
    vec3_t   view_ofs;     // Our eye position.
    float    flDuckTime;   // Time we started duck
    qboolean bInDuck;      // In process of ducking or ducked already?

    // For walking/falling
    int      flTimeStepSound; // Next time we can play a step sound
    int      iStepLeft;

    float    flFallVelocity;
    vec3_t   punchangle;

    float    flSwimTime;
    float    flNextPrimaryAttack;

    int      effects;      // MUZZLE FLASH, e.g.

    int      flags;        // FL_ONGROUND, FL_DUCKING, etc.
    int      usehull;      // 0 = regular, 1 = ducked, 2 = point hull
    float    gravity;      // Our current gravity and friction.
    float    friction;
    int      oldbuttons;   // Buttons last usercmd
    float    waterjumptime; // time left in jumping out of water cycle
    qboolean dead;         // Are we a dead player?
    int      deadflag;
    int      spectator;    // Should we use spectator physics model?
    int      movetype;     // Our movement type, NOCLIP, WALK, FLY

    int      onground;
    int      waterlevel;
    int      watertype;
    int      oldwaterlevel;

    char     sztexturename[256];
    char     chtexturetype;

    float    maxspeed;
    float    clientmaxspeed; // Player specific maxspeed

    // For mods
    int      iuser1;
    int      iuser2;
    int      iuser3;
    int      iuser4;
    float    fuser1;
    float    fuser2;
    float    fuser3;
    float    fuser4;
    vec3_t   vuser1;
    vec3_t   vuser2;
    vec3_t   vuser3;
    vec3_t   vuser4;

    // world state

    // Number of entities to clip against.
    int      numphysent;
    physent_t physents[k_max_physents];

    // Number of movement entities (ladders)
    int      nummoveent;
    // just a list of ladders
    physent_t moveents[k_max_moveents];

    // All things being rendered, for tracing against non-colliding things
    int      numvisent;
    physent_t visents[k_max_physents];

    // input to run through physics.
    usercmd_t cmd;

    // Trace results for objects we collided with.
    int      numtouch;
    pmtrace_t touchindex[k_max_physents];

    char     physinfo[k_max_physinfo_string]; // Physics info string

    movevars_t *movevars; // @annotation-exempt: abi-pod
    vec3_t   player_mins[4];
    vec3_t   player_maxs[4];

    // Common functions (host callbacks; opaque engine pointee types)
    const char *( *PM_Info_ValueForKey )( const char *s, const char *key );
    void      ( *PM_Particle )( const float *origin, int color, float life, int zpos, int zvel );
    int       ( *PM_TestPlayerPosition )( float *pos, pmtrace_t *ptrace );
    void      ( *Con_NPrintf )( int idx, const char *fmt, ... );
    void      ( *Con_DPrintf )( const char *fmt, ... );
    void      ( *Con_Printf )( const char *fmt, ... );
    double    ( *Sys_FloatTime )( void );
    void      ( *PM_StuckTouch )( int hitent, pmtrace_t *ptraceresult );
    int       ( *PM_PointContents )( float *p, int *truecontents );
    int       ( *PM_TruePointContents )( float *p );
    int       ( *PM_HullPointContents )( hull_s *hull, int num, float *p );
    pmtrace_t ( *PM_PlayerTrace )( float *start, float *end, int traceFlags, int ignore_pe );
    pmtrace_t *( *PM_TraceLine )( float *start, float *end, int flags, int usehulll, int ignore_pe );
    int       ( *RandomLong )( int lLow, int lHigh );
    float     ( *RandomFloat )( float flLow, float flHigh );
    int       ( *PM_GetModelType )( model_s *mod );
    void      ( *PM_GetModelBounds )( model_s *mod, float *mins, float *maxs );
    void     *( *PM_HullForBsp )( physent_t *pe, float *offset );
    float     ( *PM_TraceModel )( physent_t *pEnt, float *start, float *end, trace_t *trace );
    int       ( *COM_FileSize )( const char *filename );
    byte     *( *COM_LoadFile )( const char *path, int usehunk, int *pLength );
    void      ( *COM_FreeFile )( void *buffer );
    char     *( *memfgets )( byte *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufferSize );

    // Functions
    // Run functions for this frame?
    qboolean runfuncs;
    void      ( *PM_PlaySound )( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch );
    const char *( *PM_TraceTexture )( int ground, float *vstart, float *vend );
    void      ( *PM_PlaybackEventFull )( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
    pmtrace_t ( *PM_PlayerTraceEx )( float *start, float *end, int traceFlags, int ( *pfnIgnore )( physent_t *pe ) );
    int       ( *PM_TestPlayerPositionEx )( float *pos, pmtrace_t *ptrace, int ( *pfnIgnore )( physent_t *pe ) );
    pmtrace_t *( *PM_TraceLineEx )( float *start, float *end, int flags, int usehulll, int ( *pfnIgnore )( physent_t *pe ) );
    msurface_s *( *PM_TraceSurface )( int ground, float *vstart, float *vend ); // Xash3D-specific
};

// Structural tripwires; exhaustive per-field offset parity against the legacy
// headers (on both pointer widths) lives in
// tests/server/abi/test_pmove_layout.cpp.
static_assert( std::is_standard_layout_v<pmplane_t> );
static_assert( std::is_standard_layout_v<pmtrace_t> );
static_assert( std::is_standard_layout_v<physent_t> );
static_assert( std::is_standard_layout_v<playermove_t> );
// pointer-free prefixes are identical on every target:
static_assert( offsetof( physent_t, origin ) == 36 );
static_assert( offsetof( physent_t, model )  == 48 );
static_assert( offsetof( playermove_t, origin ) == 56 );
static_assert( offsetof( pmtrace_t, plane ) == 32 );
static_assert( offsetof( pmtrace_t, ent )   == 48 );

} // namespace xash::abi
