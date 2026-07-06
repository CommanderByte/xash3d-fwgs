#pragma once
// xash3dpp — vendored frozen SDK structs: entvars_t, edict_t, globalvars_t
// Legacy reference: engine/progdefs.h (entvars_t :57-218, globalvars_t
// :21-55), engine/edict.h (edict_t :25-46; FWGS leafnums union deviation
// from GoldSrc preserved verbatim), common/const.h (string_t, link_t).
// Decision ref: Q-20 EDICT_STORE — the edict array built from these
// structs is the single authoritative entity store; raw access is
// confined to the server ABI shim, the pmove bridge, and the save
// serializer.
//
// Byte-exact mirror of the structs game DLLs read and write through raw
// pointers and byte-offset arithmetic (PEntityOfEntOffset is a byte
// offset from the edict array base).  Field order, types, and implicit
// padding must never change.  tests/server/abi/test_edict_layout.cpp
// pins every field offset against the actual legacy headers.

#include <xash3dpp/abi/abi_types.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

// @annotation-exempt: abi-pod — every struct below is a byte-exact mirror of a
// frozen legacy SDK POD (entvars_t / edict_t / globalvars_t / link_t).  We do
// not own their design: field order, types, and cross-link raw pointers are
// dictated by the game-DLL ABI, so per-member @lifetime ownership annotations
// do not apply (QN abi-pod).  @thread-safety is likewise a caller/engine
// contract — these headers declare layout only, no operations.
namespace xash::abi {

// legacy: common/const.h — engine string handle (offset from pStringBase)
using string_t = std::int32_t;

// legacy: common/com_model.h link_t — area-linking list node
struct link_t
{
    link_t *prev, *next;
};

struct edict_t; // fwd — entvars_t stores edict_t* cross-links

// legacy: engine/edict.h :19-20
inline constexpr int k_max_ent_leafs_32 = 24; // "Orignally was 16"
inline constexpr int k_max_ent_leafs_16 = 48;

// legacy: engine/progdefs.h :57-218 — 123 fields, frozen layout
struct entvars_t
{
    string_t classname;
    string_t globalname;

    vec3_t   origin;
    vec3_t   oldorigin;
    vec3_t   velocity;
    vec3_t   basevelocity;
    vec3_t   clbasevelocity; // server zeroes it; client conveyor prediction
    vec3_t   movedir;

    vec3_t   angles;         // Model angles
    vec3_t   avelocity;      // angle velocity (degrees per second)
    vec3_t   punchangle;     // auto-decaying view angle adjustment
    vec3_t   v_angle;        // Viewing angle (player only)

    // For parametric entities
    vec3_t   endpos;
    vec3_t   startpos;
    float    impacttime;
    float    starttime;

    int      fixangle;       // 0:nothing, 1:force view angles, 2:add avelocity
    float    idealpitch;
    float    pitch_speed;
    float    ideal_yaw;
    float    yaw_speed;

    int      modelindex;

    string_t model;
    int      viewmodel;      // player's viewmodel
    int      weaponmodel;    // what other players see

    vec3_t   absmin;         // BB max translated to world coord
    vec3_t   absmax;
    vec3_t   mins;           // local BB min
    vec3_t   maxs;
    vec3_t   size;           // maxs - mins

    float    ltime;
    float    nextthink;

    int      movetype;
    int      solid;

    int      skin;
    int      body;           // sub-model selection for studiomodels
    int      effects;
    float    gravity;        // % of "normal" gravity
    float    friction;       // inverse elasticity of MOVETYPE_BOUNCE

    int      light_level;

    int      sequence;       // animation sequence
    int      gaitsequence;   // movement animation sequence for player
    float    frame;          // % playback position in animation sequences
    float    animtime;       // world time when frame was set
    float    framerate;      // animation playback rate (-8x to 8x)
    byte     controller[4];  // bone controller setting (0..255)
    byte     blending[2];    // blending amount between sub-sequences

    float    scale;          // sprites and models rendering scale (0..255)
    int      rendermode;
    float    renderamt;
    vec3_t   rendercolor;
    int      renderfx;

    float    health;
    float    frags;
    int      weapons;        // bit mask for available weapons
    float    takedamage;

    int      deadflag;
    vec3_t   view_ofs;       // eye position

    int      button;
    int      impulse;

    edict_t *chain;          // linked-list pointer @annotation-exempt: abi-pod
    edict_t *dmg_inflictor; // @annotation-exempt: abi-pod
    edict_t *enemy; // @annotation-exempt: abi-pod
    edict_t *aiment;         // entity pointer when MOVETYPE_FOLLOW @annotation-exempt: abi-pod
    edict_t *owner; // @annotation-exempt: abi-pod
    edict_t *groundentity; // @annotation-exempt: abi-pod

    int      spawnflags;
    int      flags;

    int      colormap;       // lowbyte topcolor, highbyte bottomcolor
    int      team;

    float    max_health;
    float    teleport_time;
    float    armortype;
    float    armorvalue;
    int      waterlevel;
    int      watertype;

    string_t target;
    string_t targetname;
    string_t netname;
    string_t message;

    float    dmg_take;
    float    dmg_save;
    float    dmg;
    float    dmgtime;

    string_t noise;
    string_t noise1;
    string_t noise2;
    string_t noise3;

    float    speed;
    float    air_finished;
    float    pain_finished;
    float    radsuit_finished;

    edict_t *pContainingEntity; // @annotation-exempt: abi-pod

    int      playerclass;
    float    maxspeed;

    float    fov;
    int      weaponanim;

    int      pushmsec;

    int      bInDuck;
    int      flTimeStepSound;
    int      flSwimTime;
    int      flDuckTime;
    int      iStepLeft;
    float    flFallVelocity;

    int      gamestate;

    int      oldbuttons;

    int      groupinfo;

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
    edict_t *euser1; // @annotation-exempt: abi-pod
    edict_t *euser2; // @annotation-exempt: abi-pod
    edict_t *euser3; // @annotation-exempt: abi-pod
    edict_t *euser4; // @annotation-exempt: abi-pod
};

// legacy: engine/edict.h :25-46 (FWGS deviation from GoldSrc: leafnums is
// a union sized 96 bytes either way; GoldSrc has short leafnums[48])
struct edict_t
{
    qboolean free;
    int      serialnumber;

    link_t   area;           // linked to a division node or leaf
    int      headnode;       // -1 to use normal leaf check

    int      num_leafs;
    union
    {
        int   leafnums32[k_max_ent_leafs_32];
        short leafnums16[k_max_ent_leafs_16];
    };

    float    freetime;       // sv.time when the object was freed

    void    *pvPrivateData;  // Alloced and freed by engine, used by DLLs @annotation-exempt: abi-pod
    entvars_t v;             // C exported fields from progs
};

// legacy: engine/progdefs.h :21-55
struct globalvars_t
{
    float       time;
    float       frametime;
    float       force_retouch;
    string_t    mapname;
    string_t    startspot;
    float       deathmatch;
    float       coop;
    float       teamplay;
    float       serverflags;
    float       found_secrets;
    vec3_t      v_forward;
    vec3_t      v_up;
    vec3_t      v_right;
    float       trace_allsolid;
    float       trace_startsolid;
    float       trace_fraction;
    vec3_t      trace_endpos;
    vec3_t      trace_plane_normal;
    float       trace_plane_dist;
    edict_t    *trace_ent; // @annotation-exempt: abi-pod
    float       trace_inopen;
    float       trace_inwater;
    int         trace_hitgroup;
    int         trace_flags;
    int         changelevel;   // transition in progress when true (was msg_entity)
    int         cdAudioTrack;
    int         maxClients;
    int         maxEntities;
    const char *pStringBase; // @annotation-exempt: abi-pod

    void       *pSaveData;     // (SAVERESTOREDATA *) pointer @annotation-exempt: abi-pod
    vec3_t      vecLandmarkOffset;
};

// Structural tripwires; the exhaustive per-field offset parity against the
// legacy headers lives in tests/server/abi/test_edict_layout.cpp.
static_assert( std::is_standard_layout_v<entvars_t> );
static_assert( std::is_standard_layout_v<edict_t> );
static_assert( std::is_standard_layout_v<globalvars_t> );
static_assert( sizeof( link_t ) == 2 * sizeof( void * ));
static_assert( sizeof( edict_t{}.leafnums32 ) == 96 );
static_assert( sizeof( edict_t{}.leafnums16 ) == 96 );
static_assert( offsetof( entvars_t, classname ) == 0 );
static_assert( offsetof( entvars_t, origin ) == 8 );
static_assert( offsetof( edict_t, v ) == ( sizeof( void * ) == 8 ? 144 : 128 ));

} // namespace xash::abi
