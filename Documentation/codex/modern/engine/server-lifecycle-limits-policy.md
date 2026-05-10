# Server Lifecycle Limits Policy

Phase 103 extracts the numeric policy around server capacity setup while
leaving the live lifecycle machinery in `engine/server/sv_init.c`.

## Legacy Baseline

`SV_SetupClients()` performs several policy decisions before it reallocates
runtime storage:

- copies `sv_maxclients` to `svs.maxclients`;
- clamps dedicated servers to `4..MAX_CLIENTS`;
- clamps listen/non-dedicated servers to `1..MAX_CLIENTS`;
- treats `maxclients == 1` as singleplayer and `maxclients > 1` as
  multiplayer;
- selects `SINGLEPLAYER_BACKUP` for one client and `MULTIPLAYER_BACKUP` for
  multiplayer, except `XASH_LOW_MEMORY == 2` where `SV_UPDATE_BACKUP` remains
  the compile-time singleplayer backup macro;
- computes `svs.num_client_entities` as
  `svs.maxclients * SV_UPDATE_BACKUP * NUM_PACKET_ENTITIES`;
- computes `svgame.numEntities` as `svs.maxclients + 1`.

`SV_ActivateServer()` also has a small settling-frame policy:

- with physics enabled, singleplayer runs 2 settling frames and multiplayer
  runs 8 settling frames;
- with physics disabled, it runs 1 frame;
- physics settling uses `SV_SPAWN_TIME` (`0.1`) as `sv.frametime`;
- no-physics settling uses `0.001` as `sv.frametime`.

## Modern Boundary

The modern helper owns only pure calculations:

- maxclient clamping;
- singleplayer/multiplayer classification;
- update-backup selection;
- packet-entity capacity calculation;
- game entity count calculation;
- spawn settling frame count and frame time.

Legacy code still owns:

- cvar mutation and latch feedback;
- full shutdown when maxclients changes;
- `Z_Realloc()` ownership for clients and packet entities;
- `NET_Config()` side effects;
- game DLL activation, baselines, resource lists, consistency transfer, and
  connected-client netchan resets.

## Route-Through

`engine/server/server_lifecycle_limits_adapter.*` exposes the narrow C calls
used by `sv_init.c`. The route-through preserves the previous control flow and
keeps allocation and lifecycle side effects adjacent to the legacy globals.

## Test Coverage

`tests/engine/server_lifecycle_limits.cpp` covers:

- dedicated and listen maxclient bounds;
- singleplayer versus multiplayer backup selection;
- update-mask calculation through `ServerUpdateMask()`;
- packet entity count calculation and overflow clamping for pure callers;
- game entity count calculation;
- combined capacity plans;
- spawn settling frame counts and frame times.
