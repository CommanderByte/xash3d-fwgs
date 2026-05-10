# Game DLL Bridge Baseline

Phase: 86

Legacy owner: `engine/server/sv_game.c`

Supporting ABI headers: `engine/eiface.h`, `engine/server/server.h`

## Scope

This baseline covers the server-side bridge between the engine and the loaded
game DLL. It does not cover the client DLL bridge, UI DLL bridge, renderer DLL,
or physics implementation details except where `sv_game.c` hands extension
callbacks to or from the server.

The key rule is that the game DLL ABI is an external compatibility boundary.
Modern C++ internals may sit behind it, but must not change the exported C
tables, function pointer order, structure layouts, calling conventions, or
legacy quirks without a dedicated ABI phase.

## ABI Participants

| Surface | Location | Direction | Ownership |
| --- | --- | --- | --- |
| `enginefuncs_t` | `engine/eiface.h` | Engine callbacks passed to the game DLL. | Stable ABI; callbacks currently implemented mostly in `sv_game.c` and nearby server modules. |
| `DLL_FUNCTIONS` | `engine/eiface.h` | Game DLL callbacks returned to the engine. | Stable ABI; stored in `svgame.dllFuncs`. |
| `NEW_DLL_FUNCTIONS` | `engine/eiface.h` | Optional game DLL extension callbacks. | Optional ABI; stored in `svgame.dllFuncs2` when available. |
| `GiveFnptrsToDll` | game DLL export | Engine gives callback table and globals to DLL. | Required for load success. |
| `GetEntityAPI` / `GetEntityAPI2` | game DLL exports | DLL returns `DLL_FUNCTIONS`. | At least one is required for load success. |
| `GetNewDLLFunctions` | game DLL export | DLL returns optional `NEW_DLL_FUNCTIONS`. | Optional; version mismatch clears the table. |
| `SV_SaveGameComment` | game DLL export | Optional save comment callback. | Probed by `SV_InitSaveRestore()`. |

`engine/eiface.h` warns that new `enginefuncs_t` entries must only be appended.
The local interface version is part of the compatibility surface.

## Persistent Bridge State

`svgame_static_t` in `engine/server/server.h` owns:

- user message write state: current name, index, destination, size patch
  position, rewrite state, origin/entity, trace flag, and started flag;
- loaded library handle;
- edict array and active entity count;
- movement globals, `playermove_t`, interpolation state, and pushed entity
  records;
- `globalvars_t` pointer passed to the game DLL;
- `DLL_FUNCTIONS`, `NEW_DLL_FUNCTIONS`, and `physics_interface_t`;
- server permanent memory pool and string pool.

This state is still adapter-owned. Pure modern code should consume snapshots or
operate behind C wrappers rather than include `server.h` directly.

## Load Sequence

`SV_LoadProgs()` performs the load and initialization handshake:

1. Return early when a game DLL is already loaded.
2. Assign static `playermove_t` and `globalvars_t` storage into `svgame`.
3. Allocate the server edict memory pool.
4. Load the DLL with `COM_LoadLibrary()`.
5. Resolve `GetEntityAPI`, `GetEntityAPI2`, optional `GetNewDLLFunctions`, and
   required `GiveFnptrsToDll`.
6. Apply the `BUGCOMP_PENTITYOFENTINDEX_FLAG` compatibility override before
   copying the engine callback table.
7. Pass a local copy of `enginefuncs_t` and `globalvars_t` to
   `GiveFnptrsToDll()`.
8. Query optional `NEW_DLL_FUNCTIONS`.
9. Prefer `GetEntityAPI2()` when it succeeds with the expected version; fall
   back to `GetEntityAPI()`.
10. Initialize server operator commands, studio API, optional physics API, and
    save/restore callbacks.
11. Allocate edicts, static baselines, normal baselines, and the engine string
    pool.
12. Set `host_gameloaded`, print the game description, call
    `pfnGameInit()`, initialize client movement, initialize deltas, and call
    `pfnRegisterEncoders()`.

Failure paths must free the loaded library and memory pool, clear the instance
handle, and report the missing export or API failure.

## Unload Sequence

`SV_UnloadProgs()`:

- returns if no DLL is loaded;
- deactivates the server and shuts down deltas;
- prepares extension cvars for unlinking;
- calls optional `pfnGameShutdown()`;
- clears `host_gameloaded`;
- frees static entities and baselines;
- kills operator commands and unlinks game cvars and commands;
- frees the string pool and resets the studio API;
- unloads the DLL, frees the game memory pool, and clears `svgame`.

DLL unload correctness depends on command/cvar unlinking happening before the
game module's code and pointers vanish.

## Engine Callback Table

`gEngfuncs` is the concrete `enginefuncs_t` table passed to the DLL. Its
callbacks currently group into these migration domains:

| Domain | Examples | Modernization note |
| --- | --- | --- |
| Precache/resource lookup | `pfnPrecacheModel`, `SV_SoundIndex`, `SV_GenericIndex`, `pfnPrecacheEvent` | Use resource catalog/index helpers before touching ABI callbacks. |
| Entity lifecycle | `SV_AllocEdict`, `pfnRemoveEntity`, `pfnCreateNamedEntity`, private-data functions | High risk: depends on edict layout, private data, and game constructors/destructors. |
| World and movement | `pfnSetSize`, `pfnSetOrigin`, `pfnMoveToOrigin`, `pfnWalkMove`, `pfnDropToFloor` | Tied to `sv_world.c`, `sv_phys.c`, and game physics callbacks. |
| Tracing and visibility | trace callbacks, `pfnSetFatPVS`, `pfnCheckVisibility` | Needs trace/world fixtures before migration. |
| Messages and sounds | `pfnMessageBegin/End`, write primitives, `pfnBuildSoundMsg`, `pfnStaticDecal` | Best near-term bridge candidate because several payload helpers already exist. |
| Cvars, commands, printing | `pfnCVar*`, `pfnServerCommand`, `pfnClientCommand`, `pfnAlertMessage` | Depends on command/cvar/logging migration boundaries. |
| Files, CRC, random, time | `COM_LoadFileForMe`, CRC callbacks, random callbacks, `Sys_FloatTime` | Some underlying utilities are already modernized, but the callback ABI remains C. |
| Client/player info | client key values, auth IDs, stats, fake clients, player movement | Coupled to `sv_client.c`, `sv_pmove.c`, and client state. |
| Delta/baseline/consistency | delta callbacks, instanced baseline, force unmodified | Coupled to network/delta and consistency policy. |

## Game DLL Callback Table

`DLL_FUNCTIONS` callbacks are invoked across the server:

- lifecycle and entity parsing: `pfnGameInit`, `pfnSpawn`, `pfnKeyValue`,
  `pfnServerActivate`, `pfnServerDeactivate`;
- game frame and physics: `pfnStartFrame`, `pfnThink`, `pfnTouch`,
  `pfnBlocked`, `pfnPM_*`, `pfnCmdStart`, `pfnCmdEnd`;
- client lifecycle: `pfnClientConnect`, `pfnClientPutInServer`,
  `pfnClientDisconnect`, `pfnClientUserInfoChanged`, spectator callbacks;
- save/restore: field read/write callbacks, global state callbacks,
  `pfnSave`, `pfnRestore`, level parameter callbacks;
- network snapshots: visibility setup, `pfnAddToFullPack`,
  `pfnCreateBaseline`, `pfnUpdateClientData`, `pfnGetWeaponData`;
- resource/consistency hooks: customization and inconsistent-file callbacks;
- connectionless fallback: `pfnConnectionlessPacket`.

These calls should remain behind explicit adapters while modern helpers are
being introduced.

## User Message Session

The game DLL writes server messages through `pfnMessageBegin()`,
`pfnMessageEnd()`, and the `pfnWrite*()` primitives.

Important compatibility details:

- only one active message is allowed;
- message numbers are clamped between `svc_bad` and `255`;
- system messages use negative `msg_index` values for diagnostics;
- variable-sized messages reserve a short size field that is patched on end;
- registered user messages must match their fixed size, unless variable sized;
- overlong, negative, malformed, or overflowing messages clear `sv.multicast`;
- some GoldSrc system messages are rewritten for Xash3D compatibility;
- multicast routing remains legacy-owned after message finalization;
- trace output is gated by `sv_trace_messages`.

This is the strongest candidate for a future C-compatible modern facade, but it
must preserve byte layout and rewrite behavior exactly.

## Entity And String Ownership

Edicts live in the `svgame.edicts` array allocated from `svgame.mempool`.
Private data belongs to the game DLL, but the engine allocates/frees storage and
calls optional `pfnOnFreeEntPrivateData()` before release.

The string pool backs `string_t` values and `globalvars_t::pStringBase`.
`SV_AllocString()`, `SV_MakeString()`, and `SV_GetString()` must preserve the
legacy numeric string-index model. Physics extensions may override string
allocation and lookup.

Entity parsing is also ABI-sensitive:

- `classname` is expected before other key/value pairs;
- leading-underscore utility keys may be discarded for skysphere worlds;
- `angle` can be rewritten to `angles`;
- some mod-specific origin adjustment exists under compatibility hacks;
- `pfnKeyValue()` and `pfnSpawn()` own game interpretation.

## Migration Risk

Do not migrate this bridge by moving `sv_game.c` wholesale. The safer route is:

1. document ABI and ownership;
2. extract small, target-neutral planners that do not expose C++ across the ABI;
3. route individual callbacks through adapters only after focused tests exist;
4. leave loading/unloading, edict layout, `globalvars_t`, and callback table
   ordering stable until an explicit ABI compatibility phase.
