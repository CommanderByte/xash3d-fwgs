# xash3dpp — physics subsystem
# Shared client/server player movement (pm_shared wrapper) and server physics dispatch.
# Legacy reference: pm_shared/, engine/server/sv_phys.c, sv_pmove.c, sv_move.c,
#                   engine/client/dll_int/cl_pmove.c
#
# Determinism note: client prediction and server authority must produce
# identical results.  Any floating-point change requires both sides to update.
#
# TODO: add source files and add_library target when implementation begins.
# Likely dependencies: xash3dpp_world, xash3dpp_utilities
