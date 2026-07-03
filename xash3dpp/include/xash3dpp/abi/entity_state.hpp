#pragma once
// xash3dpp — vendored frozen SDK structs: entity_state_t, clientdata_t
// Legacy reference: common/entity_state.h (Valve HLSDK, layout-frozen)
//
// Byte-exact mirror of the SDK structs read/written by game and client DLLs
// and delta-encoded on the wire.  Field order, types, and implicit padding
// must never change; the static_asserts below pin the layout on every
// platform (all members are fixed-width — no pointer-size dependence).

#include <xash3dpp/abi/abi_types.hpp>

#include <cstddef>

namespace xash::abi {

// entityType values (legacy ENTITY_NORMAL / ENTITY_BEAM)
inline constexpr int k_entity_normal = 1 << 0;
inline constexpr int k_entity_beam   = 1 << 1;

struct entity_state_t
{
    // Fields filled in by routines outside of delta compression
    int      entityType;
    int      number;       // index into cl_entities array
    float    msg_time;
    int      messagenum;   // message number of last update

    // Fields transmitted and reconstructed over the network stream
    vec3_t   origin;
    vec3_t   angles;

    int      modelindex;
    int      sequence;
    float    frame;
    int      colormap;
    short    skin;
    short    solid;
    int      effects;
    float    scale;
    byte     eflags;

    // Render information
    int      rendermode;
    int      renderamt;
    color24  rendercolor;
    int      renderfx;

    int      movetype;
    float    animtime;
    float    framerate;
    int      body;
    byte     controller[4];
    byte     blending[4];
    vec3_t   velocity;

    // Send bbox down to client for use during prediction
    vec3_t   mins;
    vec3_t   maxs;

    int      aiment;
    int      owner;        // projectile owner player index

    float    friction;
    float    gravity;

    // PLAYER SPECIFIC
    int      team;
    int      playerclass;
    int      health;
    qboolean spectator;
    int      weaponmodel;
    int      gaitsequence;
    vec3_t   basevelocity; // if standing on conveyor, e.g.
    int      usehull;      // crouched vs regular player hull
    int      oldbuttons;   // latched buttons last time state updated
    int      onground;     // -1 = in air, else pmove entity number
    int      iStepLeft;
    float    flFallVelocity;

    float    fov;
    int      weaponanim;

    // Parametric movement overrides
    vec3_t   startpos;
    vec3_t   endpos;
    float    impacttime;
    float    starttime;

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
};

static_assert( sizeof( entity_state_t ) == 340 );
static_assert( offsetof( entity_state_t, origin )      ==  16 );
static_assert( offsetof( entity_state_t, skin )        ==  56 );
static_assert( offsetof( entity_state_t, eflags )      ==  68 );
static_assert( offsetof( entity_state_t, rendermode )  ==  72 ); // pad after eflags
static_assert( offsetof( entity_state_t, rendercolor ) ==  80 );
static_assert( offsetof( entity_state_t, renderfx )    ==  84 ); // pad after color24
static_assert( offsetof( entity_state_t, velocity )    == 112 );
static_assert( offsetof( entity_state_t, team )        == 164 );
static_assert( offsetof( entity_state_t, startpos )    == 228 );
static_assert( offsetof( entity_state_t, vuser4 )      == 328 );

struct clientdata_t
{
    vec3_t   origin;
    vec3_t   velocity;

    int      viewmodel;
    vec3_t   punchangle;
    int      flags;
    int      waterlevel;
    int      watertype;
    vec3_t   view_ofs;
    float    health;

    int      bInDuck;
    int      weapons;

    int      flTimeStepSound;
    int      flDuckTime;
    int      flSwimTime;
    int      waterjumptime;

    float    maxspeed;

    float    fov;
    int      weaponanim;

    int      m_iId;
    int      ammo_shells;
    int      ammo_nails;
    int      ammo_cells;
    int      ammo_rockets;
    float    m_flNextAttack;

    int      tfstate;
    int      pushmsec;
    int      deadflag;
    char     physinfo[k_max_physinfo_string];

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
};

static_assert( sizeof( clientdata_t ) == 476 );
static_assert( offsetof( clientdata_t, viewmodel ) ==  24 );
static_assert( offsetof( clientdata_t, health )    ==  64 );
static_assert( offsetof( clientdata_t, physinfo )  == 140 );
static_assert( offsetof( clientdata_t, iuser1 )    == 396 );
static_assert( offsetof( clientdata_t, vuser4 )    == 464 );

} // namespace xash::abi
