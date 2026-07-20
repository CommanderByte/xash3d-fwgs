# xash3dpp_world — BSP spatial queries, entity linking, collision/trace, PVS/PAS.
# Legacy reference: engine/server/sv_world.c, engine/common/world.c,
#                   engine/common/pm_trace.c, pm_surface.c
#
# Promoted out of xash3dpp_server on 2026-07-20. It was written inside the
# server target for Chunk 6 convenience, but had already been built to a
# standalone shape: all five TUs carry ZERO references to ServerRuntime and are
# parameterised entirely by the injected `MoveEnv` / `LinkEnv` contexts. Legacy
# draws the same line — world.c and pm_trace.c live in engine/common/, only
# sv_world.c is server-side — and Chunk 12's client prediction is the second
# consumer that makes the split load-bearing rather than cosmetic.
#
# Layering: map_loader/content -> world -> server (and later -> client).
# Every dependency below is already an xash3dpp_server dependency and none of
# them depends on the server, so the insertion adds no cycle.
#
# HB-2 fence: clip.cpp carries the rotated-brush ULP kernel (`if ( rotated )`,
# clip.cpp:237-273, TODO(Q-18)). The promotion was a pure file move — the only
# content change in these TUs is the include path of the two headers that
# became public. No float math was touched.

add_library(xash3dpp_world STATIC
    links.cpp
    hulls.cpp
    clip.cpp
    contents.cpp
)

target_include_directories(xash3dpp_world
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_world PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_world
    # PUBLIC: these appear in world/trace.hpp + world/links.hpp signatures
    # (WorldData, StudioHitboxHull, Vec3/Matrix3x4, MoveEnv::cvars).
    PUBLIC  xash3dpp_map_loader
    PUBLIC  xash3dpp_content
    PUBLIC  xash3dpp_utilities
    PUBLIC  xash3dpp_cmd_cvar
    PRIVATE xash3dpp_core
)
