# Game DLL Callback Inventory

Phase: 86 deep audit

Primary owner: `engine/server/sv_game.c`

Supporting callers: `engine/server/*.c`, `engine/eiface.h`,
`engine/server/server.h`

## Purpose

This inventory expands the Phase 86 game DLL bridge baseline. The goal is to
make the bridge scope concrete enough that later phases can move small pieces
without rediscovering the callback map every time.

The public rule remains unchanged: existing game DLLs must continue to see the
same C ABI, callback table order, data structure layouts, calling conventions,
and compatibility quirks.

## Engine Callback Domains

`gEngfuncs` is the `enginefuncs_t` table passed to the loaded game DLL. The
table is ABI-stable; modern code may organize the implementation behind each
slot, but not reorder or reshape the slots.

| Domain | Representative callbacks | Near-term route suitability |
| --- | --- | --- |
| Precache and resource indexes | `pfnPrecacheModel`, `pfnPrecacheSound`, `pfnPrecacheGeneric`, `pfnModelIndex`, `pfnModelFrames`, `pfnPrecacheEvent`, `pfnDecalIndex` | Medium. Index policy is extractable, but actual model/sound/generic tables and filesystem probes remain server-owned. |
| Entity creation and lifecycle | `pfnCreateNamedEntity`, `pfnRemoveEntity`, `pfnPvAllocEntPrivateData`, `pfnPvEntPrivateData`, `pfnFreeEntPrivateData`, edict index/pointer helpers | High risk. Depends on `edict_t`, private data lifetime, `svgame.edicts`, DLL destructors, and compatibility flags. |
| World movement and placement | `pfnSetSize`, `pfnSetOrigin`, `pfnMoveToOrigin`, `pfnChangeYaw`, `pfnChangePitch`, `pfnWalkMove`, `pfnDropToFloor`, `pfnRunPlayerMove` | High risk. Tied to `sv_world.c`, `sv_phys.c`, player movement, traces, and fake-client semantics. |
| Tracing and visibility | `pfnTraceLine`, `pfnTraceHull`, `pfnTraceMonsterHull`, `pfnTraceModel`, `pfnTraceTexture`, `pfnSetFatPVS`, `pfnSetFatPAS`, `pfnCheckVisibility`, `pfnCanSkipPlayer` | High risk. Needs world/leaf/visibility fixtures before routing. |
| Server messages | `pfnMessageBegin`, `pfnMessageEnd`, `pfnWriteByte`, `pfnWriteChar`, `pfnWriteShort`, `pfnWriteLong`, `pfnWriteAngle`, `pfnWriteCoord`, `pfnWriteString`, `pfnWriteEntity` | Best first candidate. It is stateful, but has a clear begin/write/end model and can be tested with mock buffers. |
| Sound, decals, and static entities | `pfnBuildSoundMsg`, `pfnEmitAmbientSound`, `pfnStaticDecal`, `pfnMakeStatic`, `pfnParticleEffect`, `pfnLightStyle` | Medium. Several payload helpers now exist, but resource indexes and multicast remain adapter-owned. |
| Commands, cvars, and output | `pfnServerCommand`, `pfnServerExecute`, `pfnClientCommand`, `pfnClientPrintf`, `pfnServerPrint`, `pfnAlertMessage`, cvar registration/access callbacks | Medium. Command/cvar/log sinks are still legacy-owned; small policy and formatting helpers are safe. |
| User messages and query callbacks | `pfnRegUserMsg`, `pfnGetInfoKeyBuffer`, `pfnSetClientKeyValue`, cvar query callbacks, auth/user ID callbacks | Medium. Registry and validation logic is extractable; live client state must remain in adapters. |
| String pool | `pfnAllocString`, `pfnGetVarsOfEnt`, `SV_MakeString`, `SV_GetString`, physics string overrides | Medium-to-high. Pure policy can be tested, but `globalvars_t::pStringBase` and 64-bit compatibility storage remain ABI-sensitive. |
| Files, CRC, random, time | load/free file callbacks, CRC callbacks, random callbacks, time callbacks | Low-to-medium. Underlying utilities are partly modernized; callbacks still expose the legacy C surface. |
| Delta, baseline, and consistency | baseline callbacks, `pfnForceUnmodified`, `pfnCreateInstancedBaseline`, encoder callbacks | Medium-to-high. Depends on network delta and resource consistency policy. |
| Tutor, localization, and misc. extensions | localized string length, tutor counters, group masks, game dir, section end | Low for isolated policy, but keep public behavior exact because mods may probe edge cases. |

## Calls Into The Game DLL

The engine also calls the DLL through `svgame.dllFuncs`,
`svgame.dllFuncs2`, and `svgame.physFuncs`. These calls are spread across the
server, so the bridge is not only `sv_game.c`.

| Caller | Game DLL callbacks observed | Migration note |
| --- | --- | --- |
| `sv_game.c` | Load/unload, `pfnGameInit`, `pfnRegisterEncoders`, string/physics setup, message registration, save callback discovery | Owns the bridge handshake and should move last. |
| `sv_client.c` | connect/disconnect, put-in-server, userinfo changed, client command, kill, fake-client keyvalue/touch/use, connectionless fallback, cvar query callbacks, physics voice and restore helpers | Client lifecycle and command dispatch adapters must keep these calls explicit. |
| `sv_init.c` | baseline creation, server activate/deactivate, instanced baselines, game description | Coupled to map load and signon setup. |
| `sv_frame.c` | visibility setup, fullpack, client data, weapon data | Coupled to per-frame snapshot construction. |
| `sv_phys.c` | think, touch, blocked, start/end frame, draw callbacks, physics feature checks | Coupled to world and physics ownership. |
| `sv_pmove.c` | PM init/move, lag compensation, hull bounds, command start/end, player pre/post think, physics player movement hooks | Needs movement fixtures before routing. |
| `sv_world.c` | hull/trace hooks, trigger touch, touch, abs box, should collide | Needs world/trace fixtures before routing. |
| `sv_save.c` | save, restore, global state, fields, parms-change-level, physics restore-list hooks | Do not route until save/restore fixtures cover real game callback ordering. |
| `sv_custom.c` | player customization and inconsistent-file callbacks | Partly adjacent to completed resource/consistency helpers. |
| `sv_query.c` | game description | Low risk if kept as an adapter-provided string. |
| `sv_main.c` | physics prepare-world-frame | Keep with broader physics frame ownership. |

## Compatibility Details To Preserve

These behaviors are easy to lose during cleanup. Treat them as fixture
candidates before any route-through.

| Area | Compatibility detail |
| --- | --- |
| Model precache | Null or empty model names warn and return `0`; leading `!` marks optional models and changes missing-file fatality. |
| Model index lookup | Leading slash or backslash is stripped, slashes are normalized, and lookup is case-insensitive against the precache table. |
| Changelevel | Duplicate requests in the same spawncount are ignored; under compatibility hacks, a landmark name is truncated at the first space. |
| Entity search | `SV_FindEntityByString()` only searches selected string/model/sound fields from the entvars description list. |
| Messages | Only one active message may exist; variable-sized messages reserve a short size field; malformed, overlong, negative, or overflowing messages clear `sv.multicast`. |
| Message rewrites | `svc_goldsrc_spawnstaticsound` can be rewritten to `svc_sound` only when the bugcompat rewrite flag is active. |
| Message writes | `pfnWriteByte(-1)` intentionally writes `0xFF`; `pfnWriteEntity()` validates against `svgame.numEntities`; string writes count the trailing null. |
| Empty finale/cutscene | Empty `svc_finale` and `svc_cutscene` payloads receive an explicit null string. |
| Alert output | `at_logged` goes to the server log in multiplayer; `at_aiconsole` is suppressed unless developer verbosity is high enough. |
| Private data | Existing private data is freed before new allocation; allocation size is rounded up to a 16-byte boundary for compatibility. |
| String pool | `-str64alloc` and `-str64dup` affect 64-bit string storage; dedup is default; overflow resets allocation state rather than silently wrapping. |
| String processing | The current code normalizes newline, carriage return, and tab escapes before storing strings. |
| Entity pointer callback | `BUGCOMP_PENTITYOFENTINDEX_FLAG` swaps the `pfnPEntityOfEntIndex` behavior to the legacy-broken all-entity visibility variant. |
| User messages | Duplicate registration returns the existing message number; active servers resend registrations; invalid names or sizes fail to `svc_bad`. |
| Fake clients | Stufftext and some client-facing messages are skipped for fake clients while fake-client movement is explicitly supported. |
| Client maxspeed | Game-provided maxspeed is bounded by `svgame.movevars.maxspeed`, which differs from stock GoldSrc behavior. |
| Client key values | Local/serverinfo keys are ignored in client key-value mutation; unchanged values are no-ops; changed values mark resend flags. |
| Bad cvar query targets | Bad player targets can call game DLL cvar-query callbacks with a `"Bad Player"` error result when available. |
| Game dir | `pfnGetGameDir()` normally returns the game folder; a bugcompat path can return full root plus folder and falls back if it will not fit. |

## Route Ranking

| Ranking | Good candidates | Reason |
| --- | --- | --- |
| Start now | Enginefuncs metadata, message session state machine, user-message registry policy | Mostly table/state/payload logic and testable without real DLL loading. |
| Soon after | Text/command output policy, resource/precache policy, client info-key/cvar-query helpers | Needs live-state adapters, but behavior can be expressed as small decisions. |
| Fixture first | String pool, entity private data, entity parse/spawn | ABI-sensitive but can be isolated after fixtures pin edge cases. |
| Broad subsystem first | Trace, visibility, movement, world, save/restore runtime | These depend on larger modules and game callback ordering. |
| Last | DLL load/unload handshake and callback table publication | Owns library lifetime, globals, edicts, command/cvar unlinking, physics, save/restore setup, and ABI failure paths. |

## Audit Conclusion

The next safe lane is not a broad `sv_game.c` move. It is a staged bridge:
first metadata and message-session tests, then registry and output policy,
then resource/client info slices, then string/entity fixtures, and only then
load/unload ownership. This gives us real progress without letting C++ internals
escape across the game DLL ABI.
