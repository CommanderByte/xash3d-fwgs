# xash3dpp — renderer subsystem
# ref/ plugin ABI, GL/GLES/software backends, studio/BSP draw paths.
# Legacy reference: ref/  (gl/, gles/, soft/), engine/client/dll_int/ref_common.c,
#                   engine/ref_api.h
#
# Note: the legacy ref_api.h ABI is internal-only and will be redesigned.
# Target APIs: Vulkan, Metal, D3D12, OpenGL (compat).
#
# TODO: add source files and add_library target when implementation begins.
# Likely dependencies: xash3dpp_content, xash3dpp_utilities, xash3dpp_platform
