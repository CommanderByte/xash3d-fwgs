# Server Visibility Leaf And View Constraints

Phase 105 extracts the capacity policy around server visibility leaf storage
and portal-camera viewentity lists. It does not move BSP traversal, PVS/PAS
generation, or packet entity selection.

## Legacy Baseline

`engine/server/server.h` exposes:

- `MAX_VIEWENTS == 128` for `sv_client_t::viewentity[]`, the portal camera
  list that can merge visibility into a client frame.
- `MAX_ENT_LEAFS(ext)` selecting `MAX_ENT_LEAFS_32 == 24` for extended QBSP2
  leaf numbers and `MAX_ENT_LEAFS_16 == 48` for classic short leaf numbers.

`engine/server/sv_world.c` uses the leaf capacity while linking edicts:

- `SV_FindTouchedLeafs()` stores touched leaf clusters until capacity is full.
- Once full, it writes a `capacity + 1` marker into `ent->num_leafs`.
- `SV_LinkEdict()` detects `num_leafs > capacity`, clears the leaf array, and
  falls back to `ent->headnode` visibility.

`engine/server/sv_game.c` uses the same capacity for game-DLL visibility:

- `pfnCheckVisibility()` tests cached leaf numbers when `ent->headnode >= 0`.
- If the headnode test succeeds, it caches one leaf number and advances
  `ent->num_leafs` modulo the leaf capacity.

`engine/server/sv_frame.c` uses `MAX_VIEWENTS` while collecting
`EF_MERGE_VISIBILITY` portal cameras for the current client frame.

## Modern Boundary

The modern helper owns:

- classic versus extended entity leaf capacity;
- whether another leaf can be stored;
- the legacy overflow marker;
- whether a linked edict must fall back to headnode visibility;
- modulo advancement for cached headnode-visible leaf numbers;
- whether another portal viewentity can be stored.

Legacy code still owns:

- `edict_t` storage and its `leafnums32` / `leafnums16` union;
- BSP recursion and node/leaf selection;
- `Mod_GetPVSForPoint()`, `Mod_FatPVS()`, and `Mod_HeadnodeVisible()`;
- client frame entity selection and packet serialization;
- `pfnSetupVisibility()` and game-DLL visibility callbacks.

## Route-Through

`engine/server/server_visibility_constraints_adapter.*` exposes tiny C helpers
used by:

- `SV_FindTouchedLeafs()` for store-capacity and overflow-marker decisions;
- `SV_LinkEdict()` for headnode fallback detection;
- `SV_AddEntitiesToPacket()` for portal viewentity capacity;
- `pfnCheckVisibility()` for cached headnode leaf capacity and index cycling.

This keeps the policy visible in `src/engine/server` while leaving all live
world, PVS/PAS, and packet ownership in the legacy files.

## Test Coverage

`tests/engine/server_visibility_constraints.cpp` covers:

- extended and classic leaf capacities;
- leaf store admission;
- overflow marker and fallback threshold behavior;
- cached headnode leaf index wraparound;
- portal viewentity capacity admission.
