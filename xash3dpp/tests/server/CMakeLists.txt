# xash3dpp — server tests
# Slice subfolders (abi/, world/, physics/, clients/) will be registered
# from THIS file by relative source path — no nested CMakeLists
# (tests/map_loader precedent).
cmake_minimum_required(VERSION 3.16)

xash3dpp_add_test(test_server LIBS xash3dpp_server xash3dpp_core)
