# xash3dpp — demo subsystem
# Demo recording and playback (raw network delta format).
# Legacy reference: engine/client/cl_demo.c
#
# Note: the legacy demo format encodes raw network deltas.  Any change to the
# entity state layout or network encoding will break existing .dem files.
#
# RESERVED — no CMake target exists yet. This is a directory reservation for
# Chunk 12 (stub) / Chunk 14 (demo leaf); per Q-25 (directory-tracks-target)
# src/demo/ maps 1:1 to a target xash3dpp_demo, created only when that work
# begins. Until then this file is the plan for that target, not a build unit.
# Likely dependencies: xash3dpp_client, xash3dpp_networking, xash3dpp_filesystem
