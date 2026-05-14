# xash3dpp — server subsystem
# Edict/entity management, game DLL bridge (eiface.h ABI), frame snapshots,
# client connections, networking dispatch.
# Legacy reference: engine/server/sv_main.c, sv_init.c, sv_game.c,
#                   sv_client.c, sv_frame.c, sv_filter.c
#
# ABI note: enginefuncs_t (~150 callbacks) and entvars_t layout are FROZEN.
# The bridge implementation must not break existing game DLLs.
#
# TODO: add source files and add_library target when implementation begins.
# Likely dependencies: xash3dpp_world, xash3dpp_physics, xash3dpp_networking,
#                      xash3dpp_filesystem, xash3dpp_cmd_cvar
