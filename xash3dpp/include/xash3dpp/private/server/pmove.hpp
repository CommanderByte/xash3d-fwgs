#pragma once
// xash3dpp — server player-move bridge (Chunk 6 pmove-bridge P2)
// Legacy reference: engine/server/sv_pmove.c — SV_SetupPMove (:521),
// SV_FinishPMove (:599), SV_CopyEdictToPhysEnt (:42), SV_AddLinksToPmove
// (:190), SV_AddLaddersToPmove (:282).
// Deep dive: docs/legacy-survey/deep-dive-server-physics.md §4 (pmove) / §9.
//
// This is the state bridge between the client edict (entvars) and the frozen
// playermove_t the game DLL's PM_Move mutates: SetupPMove copies the edict
// state in and gathers the world's solid/ladder/visible entities from the S6
// areanode tree; FinishPMove copies the mutated state back out.  The PM_*
// trace family the DLL calls through (physents → BSP hulls) is P3, and
// SV_RunCmd (the CmdStart→PM_Move→CmdEnd chain that drives these) is P4.
//
// Q-20: the pmove bridge is the sanctioned raw `edict->v.` access site (the
// state copy is field-for-field with legacy).  Lag compensation
// (SV_GetTrueOrigin / the interpolant) is deferred to P5 — the milestone
// gathers un-interpolated positions.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/usercmd.hpp>

namespace xash::server {

struct ServerRuntime;
struct ServerClient;

// SV_SetupPMove (sv_pmove.c:521): copy the client edict's movement state into
// rt.pmove, then gather physents (solids), moveents (ladders) and visents from
// the areanode tree within a 256-unit cube around the player.  `physinfo` is
// the client's physics-info string.  Pre: rt.pmove allocated (load_progs).
void sv_setup_pmove( ServerRuntime &rt, ServerClient &cl,
                     const ::xash::abi::usercmd_t &ucmd,
                     const char *physinfo ) noexcept;

// SV_FinishPMove (sv_pmove.c:599): copy the mutated pmove state back onto the
// client edict — position/velocity/water/duck, onground→FL_ONGROUND +
// groundentity, the show-1/3-pitch body angles, and the usehull hull resize.
void sv_finish_pmove( ServerRuntime &rt, ServerClient &cl ) noexcept;

// PM_ClearPhysEnts: drop the gathered physent/moveent/visent counts (called on
// deactivate; a no-op when rt.pmove is unallocated).
void pm_clear_phys_ents( ServerRuntime &rt ) noexcept;

// SV_InitClientMove (sv_pmove.c:442): initialise rt.pmove (server flag,
// movevars, hull-bounds table) and install the ~30-entry PM_* callback table
// the game DLL's PM_Move invokes, then call the DLL's pfnPM_Init.  Registers
// rt.pmove + the hull-bounds table on the engine bridge so the context-free
// callbacks can reach them.  Called from load_progs after the pmove allocation.
void sv_init_client_move( ServerRuntime &rt ) noexcept;

// SV_RunCmd (sv_pmove.c:887): run one usercmd through the full player-move
// chain — the speed-hack clock, the msec>50 split-recurse, then
// pfnCmdStart → PM_CheckMovingGround → viewangle latch → pfnPlayerPreThink →
// SV_PlayerRunThink → SetupPMove → pfnPM_Move → FinishPMove → touch dispatch
// (deltavelocity → SV_Impact) → pfnPlayerPostThink → pfnCmdEnd.  `random_seed`
// seeds the DLL's shared-RNG for this command.  Lag compensation (the
// interpolant save/restore) is P5.  Defined in run_cmd.cpp.
void sv_run_cmd( ServerRuntime &rt, ServerClient &cl,
                 const ::xash::abi::usercmd_t &ucmd, int random_seed ) noexcept;

} // namespace xash::server
