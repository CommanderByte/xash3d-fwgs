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

# PRIVATE: engine_funcs.cpp calls into host/core, but no abi public header
# exposes their types (engine_context_accessor.hpp forward-declares only).
# The accessor STATE lives in xash3dpp_host — the link is one-way abi → host
# (D-1 dependency hardening, 2026-07-06; see docs/design/layer-model.md).
target_link_libraries(xash3dpp_abi
    PRIVATE xash3dpp_core
    PRIVATE xash3dpp_host
)
