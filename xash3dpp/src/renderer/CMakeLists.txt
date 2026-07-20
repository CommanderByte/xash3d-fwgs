# xash3dpp — renderer subsystem
# ref/ plugin ABI, GL/GLES/software backends, studio/BSP draw paths.
# Legacy reference: ref/  (gl/, gles/, soft/), engine/client/dll_int/ref_common.c,
#                   engine/ref_api.h
#
# Note: the legacy ref_api.h ABI is internal-only and will be redesigned.
# Target APIs: Vulkan, Metal, D3D12, OpenGL (compat).
#
# RESERVED — no CMake target exists yet. This is a directory reservation for
# Chunk 13 (renderer); per Q-25 (directory-tracks-target) src/renderer/ maps
# 1:1 to a target xash3dpp_renderer, created only when Chunk 13 begins. Until
# then this file is the plan for that target, not a build unit.
# Likely dependencies: xash3dpp_content, xash3dpp_utilities, xash3dpp_platform
