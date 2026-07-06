# xash3dpp — content pipeline (Chunk 7): model loaders + image codec library.
# Legacy reference: engine/common/model.c, mod_studio.c, mod_sprite.c,
#   mod_alias.c (models); engine/common/imagelib/ (images).
# Boundary: docs/boundaries/content-boundary.md
#
# Two targets per the Q-11 satellite verdict (boundary "Satellite components"):
#   • xash3dpp_imagelib — the image codec library. SEPARATE target: independent
#     file formats, its own external dep (miniz, for PNG — added with O-2), and
#     useful without the model cache (renderer textures, screenshots, sprays).
#   • xash3dpp_content  — the model handle cache + non-brush format loaders.
# Subfolders are split at scaffold time by concern (imagelib/ vs model/).
cmake_minimum_required(VERSION 3.16)

# --- imagelib (satellite target) -------------------------------------------
add_library(xash3dpp_imagelib STATIC
    imagelib/imagelib.cpp
    imagelib/codec_wad.cpp
    imagelib/codec_tga.cpp
    imagelib/codec_bmp.cpp
    imagelib/codec_dds.cpp
    imagelib/codec_ktx2.cpp
)

target_include_directories(xash3dpp_imagelib
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_imagelib PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_imagelib
    PUBLIC xash3dpp_utilities
    PUBLIC xash3dpp_memory
    PUBLIC xash3dpp_core
)
# TODO(Chunk 7, H-3/O-2): PRIVATE xash3dpp_miniz for PNG inflate/deflate.

# --- content (model handle cache + non-brush loaders) ----------------------
add_library(xash3dpp_content STATIC
    model/model_cache.cpp
    model/studio.cpp
    model/formats.cpp
)

target_include_directories(xash3dpp_content
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_content PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_content
    PUBLIC xash3dpp_utilities
    PUBLIC xash3dpp_memory
    PUBLIC xash3dpp_core
    PUBLIC xash3dpp_filesystem
)
# TODO(Chunk 7): PUBLIC xash3dpp_imagelib (skin/miptex decode, O-2) and
#   PUBLIC xash3dpp_map_loader (brush-model dispatch seam, boundary OQ-3) —
#   wired in when those code paths land, not before (the stub uses neither).
