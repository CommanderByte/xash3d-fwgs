#pragma once
// xash3dpp — server physics driver + fixed-step frame loop (Chunk 6 S8)
// Legacy reference: engine/server/sv_phys.c — SV_Physics (:1812),
// SV_Physics_Entity (:1722) + the MOVETYPE_* dispatchers, SV_PushMove
// (:899) / SV_PushRotate (:1014) pusher stack, SV_FlyMove (:592),
// SV_AddGravity (:737), SV_CheckVelocity (:120); engine/server/sv_main.c —
// SV_UpdateMovevars (:189), SV_RunGameFrame (:602), SV_PrepWorldFrame
// (:550), SV_IsSimulating (:572), Host_ServerFrame (:678).
// Deep dive: docs/legacy-survey/deep-dive-server-physics.md §2,
//            deep-dive-server-world-frame.md §5.
//
// The player-move bridge (sv_pmove.c) shares this frame path; its entry
// points live in pmove.hpp.  Q-20: the physics code reads/writes entvars
// through EntityView; the pmove bridge is the raw-access exception.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <cstddef>

namespace xash::abi {
struct edict_t; // fwd (edict.hpp)
} // namespace xash::abi

namespace xash::server {

struct ServerRuntime;
struct SvTrace; // world_trace.hpp — engine-internal trace_t (kernel result + ent)

// SV_UpdateBaseVelocity (sv_phys.c:162): conveyor-belt momentum handshake —
// fold a moving ground entity's velocity into the rider's basevelocity.
// Exposed for the pmove run chain (PM_CheckMovingGround, run_cmd.cpp).
void update_base_velocity( ServerRuntime &rt, ::xash::abi::edict_t *ent ) noexcept;

// SV_Impact (sv_phys.c:299): two entities touched — dispatch both pfnTouch
// directions (group-mask gated, SOLID_NOT suppressed).  Exposed for the pmove
// run chain's touch dispatch (run_cmd.cpp).
void sv_impact( ServerRuntime &rt, ::xash::abi::edict_t *e1,
                ::xash::abi::edict_t *e2, const SvTrace &trace ) noexcept;

// SV_UpdateMovevars (sv_main.c:189): mirror the sv_* physics cvars into
// rt.movevars (and clamp sv_zmax).  `initialize` is the spawn-time fill —
// legacy also delta-broadcasts the changes to clients on the non-initialize
// path (S9 seam here).
void sv_update_movevars( ServerRuntime &rt, bool initialize ) noexcept;

// SV_Physics (sv_phys.c:1812): StartFrame → per-entity movetype dispatch
// (skipping the client slots) → force_retouch decrement → lightstyle
// animation → framecount++.  Runs one fixed physics step at rt.level.frametime.
void sv_physics( ServerRuntime &rt ) noexcept;

// SV_PrepWorldFrame (sv_main.c:550): clear EF_MUZZLEFLASH|EF_NOINTERP on
// every live entity before the next frame's logic.
void sv_prep_world_frame( ServerRuntime &rt ) noexcept;

// SV_IsSimulating (sv_main.c:572): dedicated servers always simulate; the
// listen-server freeze/pause/background logic is an OQ-4 client-hook seam.
[[nodiscard]] bool sv_is_simulating( const ServerRuntime &rt ) noexcept;

// SV_RunGameFrame (sv_main.c:602): the fixed-`sv_fps` residual accumulator
// (with the 1/(sv_fps-0.01) FP fudge) or the one-step-per-host-frame path.
// Returns false when zero physics frames ran (the early-return quirk).
[[nodiscard]] bool sv_run_game_frame( ServerRuntime &rt, float sv_fps ) noexcept;

// Host_ServerFrame (sv_main.c:678): the per-host-frame server tick.  Wires
// the S8 pieces (movevars refresh → game frame → prep world frame); the
// client/networking steps (ReadPackets, SendClientMessages, timeouts,
// master heartbeat) are S9 seams.  `host_frametime` is the elapsed host
// wall-clock delta the host layer measures.
void host_server_frame( ServerRuntime &rt, double host_frametime ) noexcept;

} // namespace xash::server
