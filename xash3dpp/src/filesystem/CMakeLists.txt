cmake_minimum_required(VERSION 3.21)

# Determine platform source.
if(WIN32)
    set(XASH_FS_PLATFORM_SRC platform/win32.cpp)
elseif(ANDROID)
    set(XASH_FS_PLATFORM_SRC platform/posix.cpp platform/android.cpp)
else()
    set(XASH_FS_PLATFORM_SRC platform/posix.cpp)
endif()

# Optional VFS009 compatibility shim (on by default for tools / legacy clients).
option(XASH_VFS009_SHIM "Build IFileSystem009 compatibility shim" ON)

if(XASH_VFS009_SHIM)
    set(XASH_FS_VFS009_SRC vfs009/vfs009.cpp)
else()
    set(XASH_FS_VFS009_SRC "")
endif()

# Optional Android AAsset backend.
if(ANDROID)
    set(XASH_FS_ANDROID_SRC backends/android_backend.cpp)
else()
    set(XASH_FS_ANDROID_SRC "")
endif()

add_library(xash3dpp_filesystem STATIC
    filesystem.cpp
    file.cpp
    ci_directory.cpp

    backends/dir_backend.cpp
    backends/pak_backend.cpp
    backends/wad_backend.cpp
    backends/zip_backend.cpp
    backends/pk3dir_backend.cpp
    ${XASH_FS_ANDROID_SRC}

    ${XASH_FS_PLATFORM_SRC}
    ${XASH_FS_VFS009_SRC}
)

target_include_directories(xash3dpp_filesystem
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_filesystem PUBLIC cxx_std_20)

target_link_libraries(xash3dpp_filesystem PUBLIC xash3dpp_utilities xash3dpp_memory)

if(ANDROID)
    target_link_libraries(xash3dpp_filesystem PRIVATE android log)
    target_compile_definitions(xash3dpp_filesystem PRIVATE XASH_ANDROID)
endif()

if(XASH_VFS009_SHIM)
    target_compile_definitions(xash3dpp_filesystem PRIVATE XASH_VFS009_SHIM)
endif()

# zlib / miniz — required for ZIP/PK3 and deflated WADs.
target_link_libraries(xash3dpp_filesystem PRIVATE xash3dpp_miniz)
