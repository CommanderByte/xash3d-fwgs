# xash3dpp_map_loader — server-driven map-load FSM (Resolved-decision OQ-2)
# Legacy reference: engine/common/host_state.c

add_library(xash3dpp_map_loader STATIC
    map_loader.cpp
)

target_include_directories(xash3dpp_map_loader
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_map_loader PUBLIC cxx_std_20)

target_link_libraries(xash3dpp_map_loader
    PUBLIC  xash3dpp_memory
    PRIVATE xash3dpp_core
)
