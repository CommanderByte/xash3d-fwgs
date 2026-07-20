# xash3dpp — physics subsystem
# Shared client/server player movement (pm_shared wrapper).  NOT server physics
# dispatch — see the split below.
# Legacy reference: pm_shared/, engine/common/pm_trace.c, pm_surface.c,
#                   engine/client/dll_int/cl_pmove.c
#
# Determinism note: client prediction and server authority must produce
# identical results.  Any floating-point change requires both sides to update.
#
# ---------------------------------------------------------------------------
# Scope decided 2026-07-20 (audit follow-up), alongside the xash3dpp_world
# promotion.  Legacy splits player movement the same way this target does:
# engine/common/pm_trace.c + pm_surface.c are shared, engine/server/sv_phys.c +
# sv_pmove.c + sv_move.c are server-only.  Measured against our tree:
#
#   SHARED (belongs here)                 ServerRuntime refs
#     server/physics/pm_trace.cpp    800 lines   0
#     server/physics/init_client_move.cpp  495   2  (entry signature only)
#     server/physics/movevars.cpp    105         2  (entry signature only)
#
#   SERVER-ONLY (stays in xash3dpp_server)
#     server/physics/physics.cpp    1808 lines  39   SV_Physics dispatch
#     server/physics/run_cmd.cpp     281         3   usercmd execution
#     server/physics/pmove.cpp       541         7   the sv_pmove bridge
#
# The target is NOT created yet, and the blocker is a design decision that
# belongs to Chunk 11 rather than to a mechanical move:
#
#   pm_trace.cpp needs the EDICT STORE, not just edicts.  It resolves
#   `pe->info` back to an edict through `env.arena->edict_num(...)`
#   (pm_trace.cpp:92-94), and EdictArena is server-owned by Q-20 — the
#   ABI-exact store is the server's single edict store.  This is why
#   xash3dpp_world extracted cleanly and this did not: world takes `edict_t*`
#   it is handed and never asks where they came from.
#
#   Resolving it means one of: (a) move EdictArena to a lower layer, which
#   re-opens Q-20; or (b) give PmTraceEnv an index->edict lookup interface,
#   matching the IModelResolver / IClipHooks pattern MoveEnv already uses —
#   but that puts a virtual call in a byte-exact trace inner loop and wants
#   parity review, not a drive-by.
#
#   init_client_move.cpp and movevars.cpp additionally take `ServerRuntime &`
#   in their entry signatures and would need narrowing first — which is
#   exactly Chunk 11's entry-gate item 4.
#
# TODO(chunk11): create the target once the lookup seam above is decided.
# Dependencies will be: xash3dpp_world, xash3dpp_map_loader, xash3dpp_utilities.
