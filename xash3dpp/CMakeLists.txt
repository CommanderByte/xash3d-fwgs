cmake_minimum_required(VERSION 3.16)
project(xash3dpp C CXX)

set(CMAKE_CXX_STANDARD 20)
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

# miniz: use the repo-root public/miniz.c until xash3dpp/3rdparty/ is populated.
# The INTERFACE include makes <miniz.h> available to any target that links this.
add_library(xash3dpp_miniz STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../public/miniz.c)
target_include_directories(xash3dpp_miniz INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../public)

add_subdirectory(src/filesystem)
add_subdirectory(src/host)
add_subdirectory(src/launcher)

option(XASH3DPP_BUILD_TESTS "Build unit tests" ON)
if(XASH3DPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/utilities)
    add_subdirectory(tests/filesystem)
    add_subdirectory(tests/memory)
    add_subdirectory(tests/platform)
endif()
