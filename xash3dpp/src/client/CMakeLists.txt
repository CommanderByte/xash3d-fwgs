# xash3dpp — client subsystem
# Client state machine, network parse, entity/delta management, prediction,
# HUD/screen, client DLL bridge (cdll_int.h ABI).
# Legacy reference: engine/client/cl_main.c, cl_frame.c, cl_parse*, cl_view.c,
#                   cl_efx.c, cl_tent.c, dll_int/cl_game.c
#
# ABI note: cdll_int.h / cdll_exp.h callbacks are FROZEN.
#
# RESERVED — no CMake target exists yet. This is a directory reservation for
# Chunk 12 (client); per Q-25 (directory-tracks-target) src/client/ maps 1:1 to
# a target xash3dpp_client, created only when Chunk 12 begins. Until then this
# file is the plan for that target, not a build unit.
# Likely dependencies: xash3dpp_networking, xash3dpp_world, xash3dpp_physics,
#                      xash3dpp_renderer, xash3dpp_filesystem
