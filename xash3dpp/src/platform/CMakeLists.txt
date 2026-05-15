cmake_minimum_required(VERSION 3.16)

# Select platform-specific source.
if(WIN32)
    set(XASH_PLATFORM_SRC win32.cpp)
else()
    set(XASH_PLATFORM_SRC posix.cpp)
endif()

add_library(xash3dpp_platform STATIC
    ${XASH_PLATFORM_SRC}
)

target_include_directories(xash3dpp_platform
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_platform PUBLIC cxx_std_20)

# xash3dpp_utilities is used internally for path normalisation (PRIVATE — not
# leaked into xash3dpp_platform's public API).
target_link_libraries(xash3dpp_platform PRIVATE xash3dpp_utilities)

# POSIX: dlopen/dlsym/dlclose live in libdl on Linux; on macOS they are in libc.
if(UNIX AND NOT APPLE)
    target_link_libraries(xash3dpp_platform PRIVATE dl)
endif()
