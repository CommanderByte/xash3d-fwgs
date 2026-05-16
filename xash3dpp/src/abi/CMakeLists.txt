# xash3dpp_abi — C ABI direct-export shims that legacy DLLs link against.
# Legacy reference: engine/common/host.c (GAME_EXPORT Host_Error)
#
# Decision ref: docs/boundaries/host-boundary.md  Resolved-decision OQ-10

add_library(xash3dpp_abi STATIC
    engine_funcs.cpp
)

target_include_directories(xash3dpp_abi
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_abi PUBLIC cxx_std_20)

target_link_libraries(xash3dpp_abi
    PUBLIC  xash3dpp_core
    PUBLIC  xash3dpp_host
)
