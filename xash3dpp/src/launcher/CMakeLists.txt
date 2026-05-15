add_executable(xash3dpp main.cpp)

target_compile_features(xash3dpp PRIVATE cxx_std_20)

target_link_libraries(xash3dpp PRIVATE xash3dpp_host)

if(WIN32)
    # Use WinMain entry point on Windows.
    set_target_properties(xash3dpp PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
