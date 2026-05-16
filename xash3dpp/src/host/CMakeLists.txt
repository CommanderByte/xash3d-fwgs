add_library(xash3dpp_host STATIC host.cpp)

target_include_directories(xash3dpp_host
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_host PUBLIC cxx_std_20)

target_link_libraries(xash3dpp_host
    PUBLIC
        xash3dpp_memory
        xash3dpp_filesystem
        xash3dpp_core
    PRIVATE
        xash3dpp_platform
        xash3dpp_cmd_cvar
)
