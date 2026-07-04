# xash3dpp — map_loader tests
# Satellite subfolders (bsp/, pvs/, trace/) are registered from THIS file by
# relative source path — no nested CMakeLists (tests/networking precedent).
cmake_minimum_required(VERSION 3.16)

add_executable(test_map_loader test_map_loader.cpp)
target_link_libraries(test_map_loader PRIVATE xash3dpp_map_loader)
add_test(NAME test_map_loader COMMAND test_map_loader)
