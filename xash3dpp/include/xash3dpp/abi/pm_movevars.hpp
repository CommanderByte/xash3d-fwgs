#pragma once
// xash3dpp — vendored frozen SDK struct: movevars_t
// Legacy reference: common/pmove.h (layout-frozen; legacy pins 160 bytes via
// STATIC_CHECK_SIZEOF on both 32- and 64-bit targets)

#include <xash3dpp/abi/abi_types.hpp>

#include <cstddef>

// @annotation-exempt: abi-pod — movevars_t is a byte-exact, pointer-free mirror
// of the frozen SDK struct; the QN annotation matrix does not apply and
// @thread-safety is a caller contract (decisions-style QN).
namespace xash::abi {

struct movevars_t
{
    float    gravity;
    float    stopspeed;
    float    maxspeed;
    float    spectatormaxspeed;
    float    accelerate;
    float    airaccelerate;
    float    wateraccelerate;
    float    friction;
    float    edgefriction; // goldsrc binary compat
    float    waterfriction;
    float    entgravity;

    // goldsrc additions
    float    bounce;
    float    stepsize;
    float    maxvelocity;
    float    zmax;
    float    waveHeight;
    qboolean footsteps;
    char     skyName[32];
    float    rollangle;
    float    rollspeed;
    vec3_t   skycolor;
    vec3_t   skyvec;

    // xash extensions
    int      features;
    int      fog_settings;
    float    wateralpha;
    vec3_t   skydir;   // unused
    float    skyangle; // unused
};

static_assert( sizeof( movevars_t ) == 160 );
static_assert( offsetof( movevars_t, footsteps ) ==  64 );
static_assert( offsetof( movevars_t, skyName )   ==  68 );
static_assert( offsetof( movevars_t, features )  == 132 );
static_assert( offsetof( movevars_t, skyangle )  == 156 );

} // namespace xash::abi
