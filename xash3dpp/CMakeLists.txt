cmake_minimum_required(VERSION 3.16)
project(xash3dpp C CXX)

# C++23 is required for std::expected<T,E>, introduced by the sockets layer
# (include/xash3dpp/networking/errors.hpp) at the start of Chunk 2 (networking).
# Minimum required toolchains: MSVC 19.34+, GCC 12+ (libstdc++), Clang 16+ (libc++).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd / VS Code IntelliSense.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No exceptions, no RTTI — matches legacy engine policy
if(MSVC)
    add_compile_options(/EHs-c- /GR-)
else()
    add_compile_options(-fno-exceptions -fno-rtti)
endif()

add_subdirectory(src/utilities)
add_subdirectory(src/memory)
add_subdirectory(src/platform)
add_subdirectory(src/core)

# miniz: use the repo-root public/miniz.c until xash3dpp/3rdparty/ is populated.
# The INTERFACE include makes <miniz.h> available to any target that links this.
add_library(xash3dpp_miniz STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../public/miniz.c)
target_include_directories(xash3dpp_miniz INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../public)

add_subdirectory(src/filesystem)
add_subdirectory(src/cmd_cvar)
add_subdirectory(src/networking)
add_subdirectory(src/map_loader)
add_subdirectory(src/host)
add_subdirectory(src/abi)
add_subdirectory(src/launcher)

option(XASH3DPP_BUILD_TESTS "Build unit tests" ON)
if(XASH3DPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/utilities)
    add_subdirectory(tests/filesystem)
    add_subdirectory(tests/memory)
    add_subdirectory(tests/platform)
    add_subdirectory(tests/core)
    add_subdirectory(tests/cmd_cvar)
    add_subdirectory(tests/networking)
    add_subdirectory(tests/host)
endif()
