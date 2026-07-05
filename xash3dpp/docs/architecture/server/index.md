# server — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `server/server.hpp` | `xash::server` | `Server`, `ServerInitParams`, `ServerStats` |

`Server` derives from `::xash::ILevelChangeExecutor` (`map_loader`), so its
`exec_load_level` / `exec_load_game` / `exec_change_level` overrides are the
public entry the MapLoader FSM drives.

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/server/engine_bridge.hpp` | `EngineBridge` (the state behind the 159 slots), `install_engine_bridge` / `engine_bridge`, `build_engine_table`, `alloc_private_data`, `reset_external_cvars`, `HostErrorHook` |
| `private/server/edict_arena.hpp` | `EdictArena` — the Q-20 single authoritative `edict_t` store; alloc/free/init, private-data blocks, index/offset contract |
| `private/server/entity_view.hpp` | `EntityView` — the zero-cost typed entvars accessor seam; `to_vec3`/`store_vec3`/`vec_axis` helpers |
| `private/server/string_pool.hpp` | `StringPool` — `string_t ↔ char*` dual-arena (dynamic/static); `StringPoolStats` |
| `private/server/game_dll.hpp` | `GameDll` — the `SV_LoadProgs` export handshake; `query_hull_bounds` |
| `private/server/lifecycle.hpp` | `ServerRuntime` (the sv/svs/svgame aggregate), `ServerConfig`, `LevelState`, `PersistentState`, `PushedEnt`, `ServerState`; `load_progs`/`unload_progs`/`deactivate_server`/`spawn_server`/`activate_server`/`setup_clients`/`spawn_entities`/`parse_edict`/`load_from_file` |
| `private/server/precache.hpp` | `PrecacheTables` — the four 1-based model/sound/event/generic index registries; `PrecacheKind`, `PrecacheCaps` |
| `private/server/model_resolver.hpp` | `ModelResolver` — production `IModelResolver`: precache index → brush submodel |
| `private/server/world_links.hpp` | `WorldLinks`, `AreaNode`, `LinkEnv`, `IWorldLinkHooks`, `GroupOp`; intrusive link helpers, `edict_from_area` |
| `private/server/world_trace.hpp` | `MoveEnv`, `SvTrace`, `SvHull`, `BrushModel`, `IModelResolver`, `IClipHooks`; `move`/`move_no_ents`/`clip_move_to_entity`/`hull_for_entity`/`point_contents`/`brush_trigger_intersects` |
| `private/server/world_hooks.hpp` | `GameWorldHooks` — lifecycle-owned `IWorldLinkHooks`: `set_abs_box` / `dispatch_touch` / `brush_trigger_intersects` |
| `private/server/lightstyles.hpp` | `LightStyles`, `LightStyle`; `light_for_entity` |
| `private/server/physics.hpp` | `sv_physics`, `sv_run_game_frame`, `host_server_frame`, `sv_update_movevars`, `sv_prep_world_frame`, `sv_is_simulating`, `sv_impact`, `update_base_velocity` |
| `private/server/pmove.hpp` | `sv_setup_pmove`, `sv_finish_pmove`, `sv_init_client_move`, `sv_run_cmd`, `pm_clear_phys_ents` |
| `private/server/pm_trace.hpp` | `PmTraceEnv`, `PmIgnore`; the `pm_player_trace_ext` / `pm_test_player_position` / `pm_trace_model` / `pm_trace_line{,_ex}` / `pm_point_contents*` / `pm_stuck_touch` family |
| `private/server/clients.hpp` | `ClientMachinery`, `ServerClient`, `ClientState`, `UserMessageRegistry`/`UserMessage`/`MessageState`, `BanFilters`/`IdBan`/`IpBan`, `ServerLog`, `ServerFragmentSizer`, `IOobSink`; the connection + messaging + filter + log + query free functions |
| `private/server/snapshot.hpp` | `SnapshotState`, `ClientFrame`, `InstancedBaseline`; the baseline / gather / delta / send free functions |
| `private/server/info_string.hpp` | Info-string helpers (`info_value_for_key` / `info_set_value_for_key` / `info_remove_key` / `info_remove_prefixed_keys` / `info_is_valid`) — server-scoped until the utilities `Info_` consolidation |

## Source files

| File | Responsibility |
|------|---------------|
| `server.cpp` | `Server` pimpl: `init`/`shutdown`, `active`/`initialized`/`stats`, the `ILevelChangeExecutor` overrides (`exec_load_level` runs spawn→parse→activate; load-game/change-level are Chunk 8 stubs), `frame` |
| `abi/game_dll.cpp` | `GameDll` — dynlib load + `GiveFnptrsToDll`/`GetEntityAPI2`/`GetNewDLLFunctions` handshake, `LINK_ENTITY` dispatch, `query_hull_bounds` |
| `abi/engine_table.cpp` | The 159-slot `enginefuncs_t` population, `EngineBridge` install/access, `alloc_private_data`, `reset_external_cvars`, `COM_RandomLong/Float` RNG (`s_rng_state`) |
| `abi/edict_arena.cpp` | `EdictArena` — alloc/free/init edicts, private-data blocks, the reuse-quarantine + serialnumber + stale-field-scrub rules, index/offset arithmetic |
| `abi/string_pool.cpp` | `StringPool` — dual-arena alloc/make/get, escape processing, dedup, wrap-on-overflow |
| `lifecycle/game_host.cpp` | `load_progs` / `unload_progs` — the full DLL load/unload orchestration; `deactivate_server`, `set_server_state`, `setup_clients` |
| `lifecycle/spawn.cpp` | `spawn_server` / `activate_server` — per-level reset, world load, submodel precache, world-bridge install, settle frames |
| `lifecycle/entity_parse.cpp` | `parse_edict` / `load_from_file` / `spawn_entities` — the `{ … }` entity-string parse quirks |
| `lifecycle/precache.cpp` | `PrecacheTables` — the four index registries + late-precache notification |
| `lifecycle/model_resolver.cpp` | `ModelResolver` — lazy brush-submodel identity from precache name + `WorldData` |
| `lifecycle/world_hooks.cpp` | `GameWorldHooks` — routes `pfnSetAbsBox` / `pfnTouch` / trigger refinement to the game DLL + trace kernel |
| `world/links.cpp` | `WorldLinks` — areanode tree build, `link_edict` / `unlink_edict`, touch-link walk, leaf finding |
| `world/clip.cpp` | `move` / `move_no_ents` / `clip_move_to_entity`, rotated-brush transforms, the clip filter chain + fraction-compose quirk |
| `world/contents.cpp` | `true_point_contents` / `point_contents` / `rank_for_contents`, `brush_trigger_intersects` |
| `world/hulls.cpp` | `hull_for_bsp_entity` / `hull_for_entity` — Quake-vs-HL hull selection + origin offset |
| `world/light.cpp` | `LightStyles` (reset/set/run_frame) + `light_for_entity` (the unlit-map 255 stub) |
| `physics/physics.cpp` | `sv_physics`, `sv_run_game_frame`, `host_server_frame`, `sv_prep_world_frame`, `sv_is_simulating`, `sv_impact`, `update_base_velocity`, the movetype dispatch + pusher stack |
| `physics/movevars.cpp` | `sv_update_movevars` — mirror the `sv_*` physics cvars into `rt.movevars` |
| `physics/pmove.cpp` | `sv_setup_pmove` / `sv_finish_pmove` / `pm_clear_phys_ents` — the entvars↔`playermove_t` state bridge + physent gather |
| `physics/pm_trace.cpp` | The `PM_*` trace family over the map_loader kernel (physent-sourced hulls) |
| `physics/init_client_move.cpp` | `sv_init_client_move` — allocate `rt.pmove`, install the ~30-entry `PM_*` callback table, call `pfnPM_Init`; the pmove `RandomLong/Float` RNG (`s_pm_rng`) + `Info_ValueForKey` static buffer |
| `physics/run_cmd.cpp` | `sv_run_cmd` — the full `CmdStart → PM_Move → CmdEnd` per-usercmd chain (drives both real-client and `pfnRunPlayerMove` bot paths); P5 lag-comp interpolant is a no-op |
| `clients/client_state.cpp` | The connection state machine, `execute_client_message` / `SV_ParseClientMove`, `drop_client`, `check_timeouts`, `fake_connect`, `userinfo_changed`, challenge compute/check |
| `clients/snapshot.cpp` | `SnapshotState` alloc/reset/shutdown, `create_baselines`, the visible-entity gather + delta emit, per-client datagram + send driver |
| `clients/messages.cpp` | `reg_user_msg`, `message_begin`/`message_end`/`message_write_*`, `sv_multicast`, `playback_event_full` |
| `clients/net_io.cpp` | `read_packets` / `handle_connectionless` — the server↔`NetworkContext` OOB bridge |
| `clients/info_string.cpp` | The Info-string helpers |
| `clients/query.cpp` | `query_info` — the A2S/`info`/netinfo responders |
| `clients/filter.cpp` | `BanFilters` — `filter_check_id`/`filter_add_id`/…/`filter_check_ip`/… |
| `clients/log.cpp` | `log_printf` — timestamped line format + UDP logaddress path |

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `Server` | class (pimpl) | `server/server.hpp` | The subsystem facade; `ILevelChangeExecutor` for the MapLoader FSM |
| `ServerInitParams` | struct | `server/server.hpp` | Injected deps (cvars/fs/maps/net), game DLL path, milestone flags, host-error hook |
| `ServerStats` | struct | `server/server.hpp` | Tier-1 always-on counters (`frames_run`) + gated tiers |
| `ServerRuntime` | struct | `private/server/lifecycle.hpp` | The Q-2 aggregate replacing `sv`/`svs`/`svgame`; owned by `Server::Impl` |
| `ServerConfig` | struct | `private/server/lifecycle.hpp` | Static config injected once (game dir/dll, max_edicts, dedicated, error hook) |
| `LevelState` | struct | `private/server/lifecycle.hpp` | `server_t` subset — per-level, wiped every spawn |
| `PersistentState` | struct | `private/server/lifecycle.hpp` | `server_static_t` subset — persists across maps |
| `ServerState` | enum class | `private/server/lifecycle.hpp` | `Dead`/`Loading`/`Active` — values mirror `host_serverstate` |
| `EngineBridge` | struct | `private/server/engine_bridge.hpp` | The file-scope state the 159 ABI slots + pmove callbacks reach |
| `EdictArena` | class | `private/server/edict_arena.hpp` | The single authoritative `edict_t` array (Q-20) |
| `EntityView` | class | `private/server/entity_view.hpp` | Zero-cost typed entvars accessor (Q-20 seam) |
| `StringPool` | class | `private/server/string_pool.hpp` | Dual-arena `string_t` pool |
| `GameDll` | class | `private/server/game_dll.hpp` | Game-DLL loader + export handshake |
| `PrecacheTables` | class | `private/server/precache.hpp` | Model/sound/event/generic index registries |
| `ModelResolver` | class | `private/server/model_resolver.hpp` | Brush-submodel identity for trace hull selection |
| `WorldLinks` | class | `private/server/world_links.hpp` | Areanode spatial index + link/touch |
| `AreaNode` | struct | `private/server/world_links.hpp` | One areanode: axis/dist + trigger/solid/portal lists |
| `MoveEnv` | struct | `private/server/world_trace.hpp` | Per-call trace environment (world, resolver, area root, hooks) |
| `SvTrace` | struct | `private/server/world_trace.hpp` | Engine-internal `trace_t` (kernel result + hit entity + hitgroup) |
| `LightStyles` | class | `private/server/lightstyles.hpp` | The 256-style animation table |
| `PmTraceEnv` | struct | `private/server/pm_trace.hpp` | Per-call pmove-trace environment (physent-sourced) |
| `ClientMachinery` | struct | `private/server/clients.hpp` | `svs.clients` + user-message registry + multicast + filters + log |
| `ServerClient` | struct | `private/server/clients.hpp` | One `sv_client_t` slot |
| `ClientState` | enum class | `private/server/clients.hpp` | `Free`/`Connected`/`Spawning`/`Spawned`/`Zombie` |
| `SnapshotState` | struct | `private/server/snapshot.hpp` | `svs.baselines` + `sv.instanced` + the packet-entity ring |
| `ClientFrame` | struct | `private/server/snapshot.hpp` | One `client_frame_t` the client can delta against |
| `HostErrorHook` | alias | `private/server/engine_bridge.hpp` | `void(*)(void* ctx, const char* msg)` — the Q-5 error surface |

## Module statics (Q-2 exceptions)

| Symbol | File | Role |
|--------|------|------|
| `g_bridge` | `abi/engine_table.cpp` | The `EngineBridge*` the 159 context-free slots reach; the one deliberate global (install/detach only) |
| `s_rng_state` | `abi/engine_table.cpp` | `COM_RandomLong/Float` xorshift state (RNG-unification stub) |
| `s_pm_rng` | `physics/init_client_move.cpp` | The pmove `RandomLong/Float` xorshift state (RNG-unification stub) |
| ABI static return buffers | `abi/engine_table.cpp`, `physics/init_client_move.cpp` | `s_value[256]`/`s_empty`/static `""` returned by the pfn slots (frozen slot contract) |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_server` | STATIC | `xash3dpp_utilities`, `xash3dpp_memory`, `xash3dpp_map_loader`, `xash3dpp_networking`, `xash3dpp_cmd_cvar`, `xash3dpp_filesystem` | `xash3dpp_core`, `xash3dpp_platform` |

The target requires C++23 (`target_compile_features(xash3dpp_server PUBLIC
cxx_std_23)`). The networking/cmd_cvar/filesystem deps are `PUBLIC` because the
delta tables, cvar mirrors, and resource lists leak into the public runtime
types.

## Tests

Under `xash3dpp/tests/server/` (target `test_server` plus per-area binaries):

| Test | Area |
|------|------|
| `abi/test_edict_layout.cpp`, `test_eiface_layout.cpp`, `test_pmove_layout.cpp` | Byte-layout parity against the **real** legacy headers included in a sealed namespace |
| `abi/test_edict_arena.cpp`, `test_string_pool.cpp`, `test_engine_table.cpp`, `test_game_dll.cpp` | Arena reuse/serialnumber, string wrap/dedup, table population, the load handshake (against `fake_game_dll` MODULE doubles) |
| `lifecycle/test_game_lifecycle.cpp`, `test_spawn_server.cpp`, `test_entity_parse.cpp`, `test_precache.cpp` | Load/unload, spawn/activate, entity-string quirks, precache overflow/late paths |
| `world/test_world_links.cpp`, `test_world_trace.cpp`, `test_world_contents.cpp` | Areanode link/touch, trace composition + fraction rescale, point-contents ranking |
| `physics/test_physics.cpp` | Movetype dispatch, the fixed-step accumulator, movevars |
| `clients/test_client_state.cpp`, `test_messages.cpp`, `test_net_io.cpp`, `test_info_string.cpp`, `test_filter.cpp`, `test_log.cpp` | Connection state machine, multicast/user messages, OOB dispatch, info strings, ban filters, log format |
