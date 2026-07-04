# xash3dpp — map_loader tests
# Satellite subfolders (bsp/, pvs/, trace/) are registered from THIS file by
# relative source path — no nested CMakeLists (tests/networking precedent).
cmake_minimum_required(VERSION 3.16)

xash3dpp_add_test(test_map_loader LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_map_loader_world LIBS xash3dpp_map_loader)

xash3dpp_add_test(test_bsp_disk_format SOURCE bsp/test_disk_format.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_bsp_header SOURCE bsp/test_bsp_header.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_bsp_lumps SOURCE bsp/test_bsp_lumps.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_bsp_hulls SOURCE bsp/test_bsp_hulls.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_bsp_flags SOURCE bsp/test_bsp_flags.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_map_crc SOURCE bsp/test_map_crc.cpp LIBS xash3dpp_map_loader)

xash3dpp_add_test(test_pvs_decompress SOURCE pvs/test_pvs_decompress.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_pvs_queries SOURCE pvs/test_pvs_queries.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_phs SOURCE phs/test_phs.cpp LIBS xash3dpp_map_loader)

xash3dpp_add_test(test_point_contents SOURCE trace/test_point_contents.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_hull_trace SOURCE trace/test_hull_trace.cpp LIBS xash3dpp_map_loader)
xash3dpp_add_test(test_box_hull SOURCE trace/test_box_hull.cpp LIBS xash3dpp_map_loader)
