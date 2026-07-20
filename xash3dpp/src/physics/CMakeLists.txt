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
#   SHARED (belongs here) — the shared-deterministic trace kernel ONLY
#     server/physics/pm_trace.cpp    800 lines   0    the whole shared surface
#
#   SERVER-ONLY (stays in xash3dpp_server) — the SV half of a symmetric pair
#     server/physics/physics.cpp    1808 lines  39    SV_Physics world-sim
#     server/physics/pmove.cpp       541         7    the sv_pmove gather
#     server/physics/run_cmd.cpp     281         3    usercmd execution
#     server/physics/init_client_move.cpp  495   2    server pmove wiring (pm.server=1)
#     server/physics/movevars.cpp    105         2    server producer of a shared INPUT
#
#   Only pm_trace.cpp is shared code.  init_client_move.cpp and movevars.cpp
#   were mislabelled "shared" in the first cut (2026-07-20 am): a per-TU read
#   found init_client_move is the SERVER copy of the pmove wiring (its client
#   twin CL_InitClientMove is Chunk 12) and movevars is the server that fills a
#   shared input the client receives over the delta channel — both server-half,
#   not shared.  See boundaries/physics-boundary.md.
#
# The target is NOT created yet.  The blocker is NOT a design decision — the
# earlier "(a) reopen Q-20 / (b) virtual call in the inner loop" framing was a
# false trichotomy.  Corrected 2026-07-20 (pm read):
#
#   pm_trace.cpp has EXACTLY ONE role-owned reach in 800 lines:
#   physent_modelindex (pm_trace.cpp:94) resolves `pe->info -> edict ->
#   v.modelindex` via `env.arena->edict_num(...)`.  All the shared code wants
#   from the server is one INT (a model index), which it hands to the neutral
#   IModelResolver.  Every other PmTraceEnv field (world, models, player_bounds,
#   pusher_ext, cvars) is already neutral.
#
#   Fix (mechanical, legacy-precedented): legacy's SV_CopyEdictToPhysEnt
#   resolves the model AT GATHER TIME; our OQ-2 chose to re-resolve at trace
#   time, which is what dragged the store in.  Restore the gather-time shape in
#   neutral form — each role's gather fills a per-physent int model index
#   (server from edict->modelindex, client from cl_entity->modelindex) — and the
#   `arena` field leaves PmTraceEnv.  No virtual call, no Q-20 reopen, no
#   touching the byte-exact arithmetic.  Caveat: the client gather is Chunk 12
#   and unwritten, so the client half is reasoned from legacy, not verified here.
#
# TODO(chunk11): create the target once pm_trace's arena reach is severed as
# above.  Dependencies will be: xash3dpp_world, xash3dpp_map_loader,
# xash3dpp_content, xash3dpp_utilities, xash3dpp_cmd_cvar.
