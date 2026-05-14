add_library(xash3dpp_utilities STATIC
    string.cpp
    path.cpp
    hash.cpp
    matrix.cpp
    utf.cpp
    atlas.cpp
    build.cpp
    dynlib.cpp
    gameinfo_parser.cpp
)

target_include_directories(xash3dpp_utilities
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_utilities PUBLIC cxx_std_20)

# miniz: vendored deflate/zlib/ZIP — add once 3rdparty is populated
# target_link_libraries(xash3dpp_utilities PUBLIC miniz)
