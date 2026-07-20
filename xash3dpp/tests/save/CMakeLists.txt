cmake_minimum_required(VERSION 3.16)

# xash3dpp — save tests (Chunk 8, slice S8.1: token table + field-codec framing)
xash3dpp_add_test(test_save_codec LIBS xash3dpp_save)

# xash3dpp — save tests (Chunk 8, slice S8.2: ENTITYTABLE build/read layer)
xash3dpp_add_test(test_entity_table LIBS xash3dpp_save)

# xash3dpp — save tests (Chunk 8, slice S8.3: SaveGameState .HL1 writer core)
xash3dpp_add_test(test_hl1_writer LIBS xash3dpp_save)

# xash3dpp — save tests (Chunk 8, slice S8.4: LoadGameState .HL1 loader core)
xash3dpp_add_test(test_hl1_loader LIBS xash3dpp_save)

# xash3dpp — save tests (Chunk 8, slice S8.5: .sav container + save-directory
# + SV_GetSaveComment + .HL2/.HL3 + SAV-OQ-1)
xash3dpp_add_test(test_sav_container LIBS xash3dpp_save xash3dpp_filesystem)

# xash3dpp — save tests (Chunk 8, slice S8.6: landmark-transition machinery
# — LoadAdjacentEnts + CreateEntityTransitionList + EntityInSolid + decal hook)
xash3dpp_add_test(test_adjacent_transfer LIBS xash3dpp_save)

# xash3dpp — save tests (Chunk 8, slice S8.8: fixture tiers)
# env-gated real-retail-save loader (XASH3DPP_LEGACY_SAVE_DIR; SKIP-green when unset/empty)
xash3dpp_add_test(test_legacy_fixtures LIBS xash3dpp_save xash3dpp_filesystem)
# x64/x86 byte-identity harness (hex-pinned MD5 golden over a canonical fixture save)
xash3dpp_add_test(test_byte_identity LIBS xash3dpp_save)
