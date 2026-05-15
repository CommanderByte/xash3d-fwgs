add_library(xash3dpp_memory STATIC
    memory.cpp
)

target_include_directories(xash3dpp_memory
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_memory PUBLIC cxx_std_20)
