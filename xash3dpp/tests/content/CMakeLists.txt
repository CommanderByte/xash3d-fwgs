# xash3dpp — content pipeline tests (Chunk 7 skeleton: lifecycle smoke only).
# Functional coverage (WAD pack/unpack, studio header parse, model lookup)
# lands with the O-1/O-2 implementation.
cmake_minimum_required(VERSION 3.16)

xash3dpp_add_test(test_imagelib     LIBS xash3dpp_imagelib xash3dpp_miniz)
xash3dpp_add_test(test_content      LIBS xash3dpp_content)
xash3dpp_add_test(test_studio_bones LIBS xash3dpp_content)
