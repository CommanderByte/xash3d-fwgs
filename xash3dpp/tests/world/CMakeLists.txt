# xash3dpp_world tests — promoted out of tests/server/ with the target
# (2026-07-20).  The code under test only ever exercised MoveEnv/LinkEnv and
# never ServerRuntime, which is what made the promotion possible.
#
# These link xash3dpp_server anyway, and that is NOT a hidden dependency of the
# library: the fixtures need somewhere to allocate `edict_t`s from, and
# EdictArena is the tree's only edict allocator today (server-owned by Q-20 —
# the ABI-exact store is the server's single edict store). The world library
# itself takes edicts as `edict_t*` and never asks where they came from. If
# Chunk 12 gives the client its own entity store, these fixtures are the
# natural place to parameterise that.

xash3dpp_add_test(test_world_links SOURCE test_world_links.cpp LIBS xash3dpp_world xash3dpp_server xash3dpp_core xash3dpp_map_loader)
xash3dpp_add_test(test_world_trace SOURCE test_world_trace.cpp LIBS xash3dpp_world xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities)
xash3dpp_add_test(test_world_contents SOURCE test_world_contents.cpp LIBS xash3dpp_world xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities)
xash3dpp_add_test(test_studio_trace SOURCE test_studio_trace.cpp LIBS xash3dpp_world xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities xash3dpp_cmd_cvar xash3dpp_content)
