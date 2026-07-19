cmake_minimum_required(VERSION 3.16)

# xash3dpp — save tests (Chunk 8, slice S8.1: token table + field-codec framing)
xash3dpp_add_test(test_save_codec LIBS xash3dpp_save)

# xash3dpp — save tests (Chunk 8, slice S8.2: ENTITYTABLE build/read layer)
xash3dpp_add_test(test_entity_table LIBS xash3dpp_save)

# xash3dpp — save tests (Chunk 8, slice S8.3: SaveGameState .HL1 writer core)
xash3dpp_add_test(test_hl1_writer LIBS xash3dpp_save)
