# xash3dpp_physics — shared deterministic player-move tracing.
# Legacy reference: engine/common/pm_trace.c.
# Server gather/wiring remains in xash3dpp_server; Chunk 12 adds the real
# client-role gather and prediction consumer.

add_library(xash3dpp_physics STATIC
    pm_trace.cpp
)

target_include_directories(xash3dpp_physics
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_physics PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_physics
    # Public header value types.
    PUBLIC  xash3dpp_map_loader
    PUBLIC  xash3dpp_utilities

    # Pointer-only/incomplete public seams and implementation-only kernels.
    PRIVATE xash3dpp_world
    PRIVATE xash3dpp_content
    PRIVATE xash3dpp_cmd_cvar
    PRIVATE xash3dpp_core
)

# Static-library PRIVATE dependencies appear as $<LINK_ONLY:...> forwarding
# entries in CMake. Reject a genuinely public world edge while allowing that
# link-only implementation propagation.
get_target_property(_physics_interface_links xash3dpp_physics INTERFACE_LINK_LIBRARIES)
foreach(_edge IN LISTS _physics_interface_links)
    if(_edge STREQUAL "xash3dpp_world")
        message(FATAL_ERROR "xash3dpp_physics must not expose world publicly")
    endif()
endforeach()
