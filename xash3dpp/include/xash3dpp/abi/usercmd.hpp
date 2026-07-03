#pragma once
// xash3dpp — vendored frozen SDK struct: usercmd_t
// Legacy reference: common/q_client.h (layout-frozen; legacy pins 52 bytes
// via STATIC_CHECK_SIZEOF on both 32- and 64-bit targets)

#include <xash3dpp/abi/abi_types.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::abi {

struct usercmd_t
{
    std::int16_t  lerp_msec; // added in HL
    std::int8_t   msec;      // added in QW
    std::uint8_t  pad1;
    vec3_t        viewangles;

    // intended velocities
    float         forwardmove;
    float         sidemove;
    float         upmove;
    std::uint8_t  lightlevel;
    std::uint8_t  pad2;
    std::uint16_t buttons;   // added in QW
    std::uint8_t  impulse;

    // added in HL
    std::uint8_t  weaponselect;
    std::uint8_t  pad3[2];

    // unused HL impact stuff, left for modders
    std::int32_t  reserved[4];
};

static_assert( sizeof( usercmd_t ) == 52 );
static_assert( offsetof( usercmd_t, viewangles ) ==  4 );
static_assert( offsetof( usercmd_t, buttons )    == 30 );
static_assert( offsetof( usercmd_t, reserved )   == 36 );

} // namespace xash::abi
