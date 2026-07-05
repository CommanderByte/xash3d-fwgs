add_library(xash3dpp_host STATIC
    host.cpp
    engine_context.cpp
    engine_context_accessor.cpp
)

target_include_directories(xash3dpp_host
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_host PUBLIC cxx_std_20)

target_link_libraries(xash3dpp_host
    PUBLIC
        xash3dpp_memory
        xash3dpp_filesystem
        xash3dpp_core
        xash3dpp_map_loader
        xash3dpp_networking
        xash3dpp_server        # EngineContext embeds server::Server (Chunk 6)
    PRIVATE
        xash3dpp_platform
        xash3dpp_cmd_cvar
        # xash3dpp_abi intentionally NOT linked: the accessor state the host
        # sets lives in THIS target (engine_context_accessor.cpp); the abi
        # shim target consumes it one-way (abi → host).  D-1, 2026-07-06.
)
