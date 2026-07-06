#pragma once
// xash3dpp — vendored frozen SDK structs: event_info_t / event_state_t
// Legacy reference: common/world.h:117-135 (HL1 event queue, layout-frozen).
// These are engine-internal (never cross the game-DLL boundary), but the
// embedded event_args_t is frozen ABI, so the whole queue is vendored verbatim
// with layout static_asserts.  The server drains this queue in SV_EmitEvents;
// the client (Chunk 12) reuses the same layout for its own event ring.

#include <xash3dpp/abi/event_args.hpp>

#include <cstddef>
#include <cstdint>

// @annotation-exempt: abi-pod — event_info_t / event_state_t are byte-exact,
// pointer-free mirrors of the frozen legacy event queue; the QN annotation
// matrix does not apply and @thread-safety is a caller contract (decisions-style QN).
namespace xash::abi {

// MAX_EVENT_QUEUE (world.h:117): 16 simultaneous events max, 64-deep ring.
inline constexpr int k_max_event_queue = 64;

// event_info_t (world.h:119): one queued event awaiting emit.  `flags` is
// CLIENT-ONLY in the legacy engine (reliable-vs-not); the server never reads it
// but it is part of the frozen layout, so it is vendored verbatim.
struct event_info_t
{
    std::uint16_t index;        // word — 0 ⇒ slot unused
    std::int16_t  packet_index; // short — delta-packet entity slot, -1 ⇒ standalone
    std::int16_t  entity_index; // short — the edict this event fires from
    float         fire_time;    // non-zero ⇒ delayed fire time (client fixup)
    event_args_t  args;
    int           flags;        // CLIENT ONLY (reliable/etc.)
};

static_assert( sizeof( event_info_t ) == 88 );
static_assert( offsetof( event_info_t, fire_time ) ==  8 );
static_assert( offsetof( event_info_t, args )      == 12 );
static_assert( offsetof( event_info_t, flags )     == 84 );

// event_state_t (world.h:132): the per-client 64-deep event ring.
struct event_state_t
{
    event_info_t ei[k_max_event_queue];
};

static_assert( sizeof( event_state_t ) == 88 * k_max_event_queue );

} // namespace xash::abi
