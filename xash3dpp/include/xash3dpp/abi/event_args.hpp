#pragma once
// xash3dpp — vendored frozen SDK struct: event_args_t
// Legacy reference: common/event_args.h (Valve HLSDK, layout-frozen)

#include <xash3dpp/abi/abi_types.hpp>

#include <cstddef>

// @annotation-exempt: abi-pod — event_args_t is a byte-exact, pointer-free
// mirror of the frozen SDK struct; the QN annotation matrix does not apply and
// @thread-safety is a caller contract (decisions-style QN).
namespace xash::abi {

// flags values (legacy FEVENT_ORIGIN / FEVENT_ANGLES)
inline constexpr int k_fevent_origin = 1 << 0; // invoked with stated origin
inline constexpr int k_fevent_angles = 1 << 1; // invoked with stated angles

struct event_args_t
{
    int   flags;

    // Transmitted
    int   entindex;

    float origin[3];
    float angles[3];
    float velocity[3];

    int   ducking;

    float fparam1;
    float fparam2;

    int   iparam1;
    int   iparam2;

    int   bparam1;
    int   bparam2;
};

static_assert( sizeof( event_args_t ) == 72 );
static_assert( offsetof( event_args_t, origin )  ==  8 );
static_assert( offsetof( event_args_t, ducking ) == 44 );
static_assert( offsetof( event_args_t, bparam2 ) == 68 );

} // namespace xash::abi
