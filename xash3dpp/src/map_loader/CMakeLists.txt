# xash3dpp_map_loader — map-load FSM (Resolved-decision OQ-2) + BSP world
# loading and spatial queries (Chunk 5).
# Legacy reference: engine/common/host_state.c, mod_bmodel.c, pm_trace.c

add_library(xash3dpp_map_loader STATIC
    map_loader.cpp
    world.cpp
    bsp/bsp_loader.cpp
    bsp/bsp_lumps.cpp
    bsp/bsp_hulls.cpp
)

target_include_directories(xash3dpp_map_loader
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_map_loader PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_map_loader
    PUBLIC  xash3dpp_memory
    PUBLIC  xash3dpp_filesystem
    PUBLIC  xash3dpp_utilities
    PRIVATE xash3dpp_core
)
