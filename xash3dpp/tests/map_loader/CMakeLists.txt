# xash3dpp — map_loader tests
# Satellite subfolders (bsp/, pvs/, trace/) are registered from THIS file by
# relative source path — no nested CMakeLists (tests/networking precedent).
cmake_minimum_required(VERSION 3.16)

add_executable(test_map_loader test_map_loader.cpp)
target_link_libraries(test_map_loader PRIVATE xash3dpp_map_loader)
add_test(NAME test_map_loader COMMAND test_map_loader)

add_executable(test_bsp_disk_format bsp/test_disk_format.cpp)
target_link_libraries(test_bsp_disk_format PRIVATE xash3dpp_map_loader)
add_test(NAME test_bsp_disk_format COMMAND test_bsp_disk_format)

add_executable(test_bsp_header bsp/test_bsp_header.cpp)
target_link_libraries(test_bsp_header PRIVATE xash3dpp_map_loader)
add_test(NAME test_bsp_header COMMAND test_bsp_header)

add_executable(test_bsp_lumps bsp/test_bsp_lumps.cpp)
target_link_libraries(test_bsp_lumps PRIVATE xash3dpp_map_loader)
add_test(NAME test_bsp_lumps COMMAND test_bsp_lumps)

add_executable(test_bsp_hulls bsp/test_bsp_hulls.cpp)
target_link_libraries(test_bsp_hulls PRIVATE xash3dpp_map_loader)
add_test(NAME test_bsp_hulls COMMAND test_bsp_hulls)

add_executable(test_bsp_flags bsp/test_bsp_flags.cpp)
target_link_libraries(test_bsp_flags PRIVATE xash3dpp_map_loader)
add_test(NAME test_bsp_flags COMMAND test_bsp_flags)

add_executable(test_map_crc bsp/test_map_crc.cpp)
target_link_libraries(test_map_crc PRIVATE xash3dpp_map_loader)
add_test(NAME test_map_crc COMMAND test_map_crc)

add_executable(test_pvs_decompress pvs/test_pvs_decompress.cpp)
target_link_libraries(test_pvs_decompress PRIVATE xash3dpp_map_loader)
add_test(NAME test_pvs_decompress COMMAND test_pvs_decompress)

add_executable(test_pvs_queries pvs/test_pvs_queries.cpp)
target_link_libraries(test_pvs_queries PRIVATE xash3dpp_map_loader)
add_test(NAME test_pvs_queries COMMAND test_pvs_queries)
