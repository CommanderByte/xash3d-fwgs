# Chunk 11 physics target interface gates. The parity witness joins this
# standalone (server-free) directory in the next commit.
xash3dpp_add_test(test_physics_public_header
    SOURCE test_public_header.cpp
    LIBS xash3dpp_physics)

xash3dpp_add_test(test_physics_role_parity
    SOURCE test_role_parity.cpp
    LIBS xash3dpp_physics xash3dpp_core xash3dpp_map_loader)
