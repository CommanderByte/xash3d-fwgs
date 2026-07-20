# xash3dpp — ui subsystem
# Console draw, VGUI bridge, MainUI DLL integration.
# Legacy reference: engine/client/console.c, cl_font.c, cl_scrn.c,
#                   vgui/vgui_draw.c, dll_int/cl_gameui.c
#
# RESERVED — no CMake target exists yet. This is a directory reservation for
# Chunk 12 (stub) / Chunk 14 (ui leaf); per Q-25 (directory-tracks-target)
# src/ui/ maps 1:1 to a target xash3dpp_ui, created only when that work begins.
# Until then this file is the plan for that target, not a build unit.
# Likely dependencies: xash3dpp_renderer, xash3dpp_client, xash3dpp_platform
