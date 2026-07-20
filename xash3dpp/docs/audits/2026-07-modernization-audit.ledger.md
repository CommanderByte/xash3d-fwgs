# 2026-07 modernization audit — obligation & decision ledger

Extracted verbatim from the audit's Phase-3 lens corpus (11 lenses, the
structured output every lens agent returned). The machine-readable form is
`2026-07-modernization-audit.ledger.json` beside this file; this page is a
human index over it. Both are a SNAPSHOT of 2026-07-20 — re-running a lens
supersedes its section rather than appending to it.

`chunk` is free text as each lens wrote it (34 distinct keys, including
`HB-5 brief`, `Chunk 12`, `tooling` and similar), so the groupings below are
the lenses' own words, not a normalised schedule.

The close-out narrative flattened this data into prose and shipped an
empty adjudication table, which is why eleven obligations were reported
as ownerless and two reports cite an "audit obligations register" that
was never created. This file is that register.

| | count |
|---|---|
| recommendations | 61 |
| shape constraints | 63 |
| obligations | 100 |
| open decisions | 30 |

The close-out reported ~58 obligations and 24 open decisions; the
corpus holds 100 and 30. The lower numbers counted only L10's
forward-fit register, not the obligations the other ten lenses
raised in their own right.

## Obligations by owning chunk

### HB-5 brief — 2

- **[L1-publish-primitive]** State the count explicitly and up front: one real cross-thread structured publisher, zero production consumers. Any brief that opens by assuming a primitive is needed has skipped the question.
  - evidence: xash3dpp/src/sound/topology.cpp:416-472; Sound::channels_snapshot has no production caller (grep over src/ returns only sound.cpp:956 and tests)
- **[L1-publish-primitive]** Use sound's four mechanisms plus LockedSfxResolver as the worked examples/test cases rather than starting from a blank slate; each of the four is individually justified by a real constraint (hot vs cold path, tearing, POD vs owning) and any primitive that cannot express all four is under-specified.
  - evidence: sound.hpp:54-82; audio_command.hpp:253-258; topology.hpp:146-149,378-386; registry.hpp:150-172

### G-5 scripting runtime (no chunk number assigned) — 1

- **[L6-allocation-seam]** Do not gate the allocator-hook decision or the spike's 'allocator fidelity' axis on HB-7 — all 3 shortlisted candidates already clear the alignment bar at today's ≤8-byte payload guarantee.
  - evidence: scripting-runtime-brief.md §5.1-5.3 per-candidate allocator API notes

### G-5 scripting runtime — AngelScript path specifically — 1

- **[L6-allocation-seam]** If AngelScript is picked, its spike task is a TLS-based PoolHandle-routing shim for per-VM attribution (already scoped in §5.3) — this is unrelated to and does not require HB-7.
  - evidence: scripting-runtime-brief.md §5.3: 'its spike task is exactly the TLS-routing/size-tracking shim'

### Chunk 11 (physics) — 2

- **[L2-registry-unification]** None from this lens. The 4 `switch(movetype)` sites plus `switch(solid)`/`switch(waterlevel)` are parity-shaped by choice and are the slot the deferred physFuncs override hooks fit into — L2-R6 records that they are NOT to be table-ified when the physics boundary spec is finally written.
  - evidence: src/server/physics/physics.cpp:1530,683,1132,1143; src/server/physics/pmove.cpp:150; finding F2 (CONFIRMED leave-as-is)
- **[L8-thread-model-packet]** The chunk has NO boundary spec, so q21_scan's 16/16 is silence not health. Its threading contract is already fixed by §8.2 (single global pmove_t, players processed sequentially) and must be recorded in the spec that does not yet exist; the spec must also not introduce new global physics state, per §8.2's own rider.
  - evidence: xash3dpp/docs/design/threading-model.md:438-448 §8.2; six 0-TU skeletons (client, demo, physics, renderer, ui, world) have no boundary spec

### 11 — physics — 1

- **[L3-introspection-stats]** pm_shared is FROZEN and physics is a 0-TU skeleton with no boundary spec. When that spec is written, its stats row must state either 'no stats — Main-only, exempt per STATS_TIERS' or commit to an atomic-storage struct with a by-value accessor; it must not repeat the save-boundary.md pattern of promising a stats struct that is never built.
  - evidence: save-boundary.md:277-279 promises SaveStats 'as always-on Tier-1 atomics'; grep for the name finds it in four doc files and zero code files (F131/F134).

### 11 physics — 12

- **[L4-state-shape-g2]** The (non-existent) physics boundary spec must state where `PhysicsContext` gets the sim clock, and must not copy it. It must also declare whether the 21-function `ServerRuntime &` corridor in src/server/physics/ is in or out of Chunk 11's scope — narrowing those signatures and porting parity math in the same wave is the collision to avoid.
  - evidence: xash3dpp/src/server/physics/physics.cpp:452 (fly_move, 126 lines, 0 rt members, forwards rt), :351, :1046, :620; extension-goals.md:295 makes the per-body PhysicsContext binding
- **[L10-forward-fit-obligations-register]** OBL-11-1 — Write the physics boundary spec. The plan's own Chunk-11 entry states "full boundary spec needed at chunk start" and none exists; physics is one of six 0-TU skeletons invisible to q21_scan.
  - evidence: xash3dpp/docs/implementation-plan.md:258 (Recon/Boundary line); xash3dpp/docs/boundaries/ contains 16 files, no physics-boundary.md
- **[L10-forward-fit-obligations-register]** OBL-11-2 — Supply the production IClipHooks implementation via svgame.physFuncs / SV_InitPhysicsAPI negotiation, and RESOLVE THE DOUBLE-DEFAULT: either make the three methods pure virtual and install a static default instance so MoveEnv::hooks is never null, OR keep the out-of-line default and delete the four null-guard branches — not both. Until then the guards and custom_clip are unreachable code that reads as live.
  - evidence: xash3dpp/src/server/lifecycle/save_bridge.cpp:778-779; xash3dpp/include/xash3dpp/private/server/world_trace.hpp:113-145; guards at clip.cpp:379,415,452; out-of-line custom_clip at clip.cpp:158; MoveEnv::hooks never assigned anywhere
- **[L10-forward-fit-obligations-register]** OBL-11-3 — Supply the physint veto/override hooks (SV_AllowSaveGame, pfnCreateEntitiesInRestoreList) and the pfnSaveGlobalState/pfnRestoreGlobalState global-state provider. container_codec.hpp's ISaveGlobalState seam exists and is wired to nullptr; no physics-interface negotiation exists before Chunk 11.
  - evidence: xash3dpp/src/server/lifecycle/save_bridge.cpp:876-878; implementation-plan.md Chunk 8 'Deferred (recorded, tagged chunk11/chunk12)' line
- **[L10-forward-fit-obligations-register]** OBL-11-4 — HB-2 FENCED. Rotated-brush ULP parity ticks with pmove parity. The real anchor is the `if (rotated)` block at clip.cpp:237-273 (in-code TODO(Q-18)); decisions-architecture.md:946-954 still says clip.cpp:211 and has DRIFTED. Correct the anchor before parity work runs through it.
  - evidence: xash3dpp/src/server/world/clip.cpp:237-273 and :251; xash3dpp/docs/design/decisions-architecture.md:946-954
- **[L10-forward-fit-obligations-register]** OBL-11-5 — The client-side pmove RandomLong/RandomFloat install must reuse HB-12's ONE shared generator instance, not copy the per-TU xorshift placeholder a third/fourth time. Legacy wires the literal same COM_RandomLong/COM_RandomFloat pointer into both the engfuncs slot and the pmove slot on BOTH paths — there is no independent stream in GoldSrc, and the 'two independent streams must stay independent' premise recorded in one amendment round is refuted by the legacy source.
  - evidence: engine/server/sv_pmove.c:478-479; engine/client/dll_int/cl_pmove.c:769; engine/common/common.c:54-118 (ran1, idum + NTAB=32 shuffle); xash3dpp/src/server/physics/init_client_move.cpp:278-303,439-440; xash3dpp/src/server/abi/engine_table.cpp:1554-1583
- **[L10-forward-fit-obligations-register]** OBL-11-6 — Record the ALREADY-RATIFIED threading contract in the new spec: single global pmove_t, players processed sequentially, and the rider that no new global physics state may be introduced. This is transcription, not a decision.
  - evidence: xash3dpp/docs/design/threading-model.md:438-448 §8.2
- **[L10-forward-fit-obligations-register]** OBL-11-7 — PhysicsContext must take the sim clock from the runtime it is constructed against, never as a copied scalar refreshed by a caller. EngineBridge::sv_time is the worked example of what the copy costs.
  - evidence: xash3dpp/include/xash3dpp/private/server/engine_bridge.hpp:96; reads at engine_table.cpp:100,711,1097,2245; docs/design/extension-goals.md:295 (per-body PhysicsContext binding)
- **[L10-forward-fit-obligations-register]** OBL-11-8 — Declare explicitly whether the 21-function `ServerRuntime &` corridor in src/server/physics/ is in or out of Chunk 11's scope. Narrowing signatures and porting parity math in the same wave is the collision to avoid; either answer is acceptable, no answer is not.
  - evidence: xash3dpp/src/server/physics/physics.cpp:452 (fly_move, 126 lines, 0 rt members, forwards rt), :351, :1046, :620; L4-R5
- **[L10-forward-fit-obligations-register]** OBL-11-9 — The spec must state where the consolidated world/physics micro-predicates live (vector_is_null / bounds_intersect / check_angles / fbit — 16 definitions under 4 competing names today, 3 of which gate the HB-2 fenced block) BEFORE any new physics TU is written, or the duplication regrows.
  - evidence: xash3dpp/src/server/world/clip.cpp:30,36,43; contents.cpp:25,30; links.cpp:39; hulls.cpp:114; pm_trace.cpp:65,73; physics.cpp:65; run_cmd.cpp:48; pmove.cpp:57
- **[L10-forward-fit-obligations-register]** OBL-11-10 — The four switch(movetype) sites plus switch(solid)/switch(waterlevel) are parity-shaped BY CHOICE and are the slot the deferred physFuncs override hooks fit into. They are explicitly NOT to be table-ified.
  - evidence: xash3dpp/src/server/physics/physics.cpp:1530,683,1132,1143; pmove.cpp:150; finding F2 (CONFIRMED leave-as-is)
- **[L10-forward-fit-obligations-register]** OBL-11-11 — The spec must record that src/physics/ is an empty placeholder and the pm_shared work already lives in src/server/physics/; likewise src/world/ vs src/server/world/. Two of the six skeleton directories are factually misleading about what is unbuilt.
  - evidence: xash3dpp/src/physics/CMakeLists.txt:1-10; xash3dpp/src/world/CMakeLists.txt:1-8; actual code in xash3dpp/src/server/physics/ and xash3dpp/src/server/world/

### 11 (physics) — 2

- **[L11-subtraction]** Supply the production IClipHooks implementation via svgame.physFuncs / SV_InitPhysicsAPI negotiation, and resolve its double-default (pure-virtual + static default instance, OR out-of-line default + delete the four null guards — not both). Until then the four guard branches at clip.cpp:379/415/452 and custom_clip at clip.cpp:158 are unreachable and must be marked as a door.
  - evidence: xash3dpp/include/xash3dpp/private/server/world_trace.hpp:113-145; xash3dpp/src/server/lifecycle/save_bridge.cpp:778
- **[L11-subtraction]** The boundary spec that does not yet exist for physics must record that src/physics/ is an empty placeholder and the pm_shared work lives in src/server/physics/ — and must state where the consolidated micro-predicates from SUB-3 live before any new physics TU is written, or the duplication will regrow.
  - evidence: xash3dpp/src/physics/CMakeLists.txt:1-8; xash3dpp/src/server/physics/pmove.cpp:57

### physics (Chunk 11) — 1

- **[L7-determinism-rng]** The client-side pmove RandomLong/RandomFloat install must reuse whatever shared-instance shape HB-12 settles on, not copy-paste the existing per-TU placeholder pattern a third/fourth time
  - evidence: implementation-plan.md:258-260 (determinism deliverable); init_client_move.cpp:439-440 (existing precedent that would be copied)

### Chunk 12 (client) — 4

- **[L1-publish-primitive]** The thread-model decision must answer whether any long-lived thread exists outside src/sound/topology.cpp. HB-5's warranted/not-warranted verdict is downstream of that answer and cannot be settled before it.
  - evidence: facts.json production thread spawns = 2, both src/sound/topology.cpp (exact)
- **[L1-publish-primitive]** When client work first opens the delta pipeline, execute the snapshot_*→delta_frame_* rename per the L1-R2 vocabulary rule. Identifiers only — field widths, ordering and codec arithmetic are HB-2 fenced.
  - evidence: 165 of 308 tree-wide `snapshot` occurrences are in server; src/server/clients/snapshot.cpp alone holds 92
- **[L8-thread-model-packet]** Record the T_NetIO decision (or cite threading-model.md §3.5/§3.7 as the standing default) before the chunk starts. The render half of the current bundled precondition must be reassigned to Chunk 13.
  - evidence: xash3dpp/docs/implementation-plan.md:270 — 'The thread-model decision (whether network I/O or rendering run off-main) must be made before this chunk starts.' vs threading-model.md:165-167 §3.5 'Chunks 8/10/11/12 — Model B unchanged. No new threads.' and :173-177 §3.7 'T_NetIO is deferred until dedicated server load demonstrates a need.'
- **[L8-thread-model-packet]** Client DLL calls are permanently pinned to T_Main by the non-reentrant cdll_int.h ABI — this is a named permanent blocker, not a candidate for relaxation, and no client-side waiver may be written as class=pending-decision.
  - evidence: xash3dpp/docs/design/threading-model.md:462-467 §8.4

### 12 — client — 1

- **[L3-introspection-stats]** The thread-model decision is a stated precondition for Chunk 12, and it is also what determines whether any stats struct needs off-Main reads at all. Decide it BEFORE any ClientStats is shaped, or the shape will be guessed. If client introspection is wanted for G-4, it needs an entity-list channel that does not exist (SC-7) — EntityView cannot serve it.
  - evidence: private/server/entity_view.hpp:42 (private header); implementation-plan.md:601-609 names EntityView as an HB-6 channel

### 12 client — 21

- **[L4-state-shape-g2]** Precondition: the cvar storage-ownership decision (L4-R2) must be ratified BEFORE the client DLL's cvar surface is wired, or a third registry gets written. FCVAR_CLIENTDLL already exists in cmd_cvar's flag enum, which is how close this is.
  - evidence: xash3dpp/src/cmd_cvar/cvar_ops.cpp:88-89 (owner_flags derived from FCVAR_EXTDLL|FCVAR_CLIENTDLL|FCVAR_GAMEUIDLL|FCVAR_REFDLL); xash3dpp/include/xash3dpp/private/server/engine_bridge.hpp:120
- **[L4-state-shape-g2]** The stated thread-model precondition should be answered with the L4 finding in hand: the tree's context-free reach is already confined to two files, so a thread model that keeps the ABI shim main-pinned costs almost nothing structurally. The expensive part is not P-3 — it is the cvar registry and the 21-function physics corridor.
  - evidence: grep for `g_bridge|engine_bridge()` across xash3dpp/src returns exactly src/server/abi/engine_table.cpp and src/server/physics/init_client_move.cpp
- **[L10-forward-fit-obligations-register]** OBL-12-1 — Record the T_NetIO decision, or cite threading-model.md §3.5/§3.7 as the standing default (Model B unchanged; T_NetIO deferred to the two unfired §7.4 triggers). The render half of the current bundled precondition must be reassigned to Chunk 13, where Q-6/Q-10 already put the backend choice it depends on.
  - evidence: xash3dpp/docs/implementation-plan.md:270; threading-model.md:165-167 §3.5, :173-177 §3.7, §7.4; decisions-architecture.md:389-390 (Q-6), :507-508 (Q-10)
- **[L10-forward-fit-obligations-register]** OBL-12-2 — Ratify the cvar storage-ownership decision BEFORE wiring the client DLL's cvar surface, or a THIRD cvar registry gets written. FCVAR_CLIENTDLL already exists in the flag enum. Today a game DLL cannot read a single engine cvar; four `TODO(chunk6-S7): fall through to the engine cvar registry` markers mark the gap.
  - evidence: xash3dpp/src/server/abi/engine_table.cpp:1199,1212,1223,1799; xash3dpp/src/cmd_cvar/cvar_ops.cpp:88-89; include/xash3dpp/private/server/engine_bridge.hpp:120
- **[L10-forward-fit-obligations-register]** OBL-12-3 — Sound's deferred list, all tagged chunk12: the soundfade curve (S_UpdateSoundFade port) so MixConfigSnapshot::gate.soundfade_gain gets a real producer instead of constant 1.0; the MixGateSnapshot producer (host.status / cls.key_dest / cl.paused / cl.background / CL_IsInGame / Host_IsSinglePlayerGame) — every gate is false today and the focus mute is live but permanently dormant; pitch_mult (sys_timescale); ambient channels + S_ClearBuffer; the sfx handle-0 `*default` reservation (precache wiring); soundlist/music bodies.
  - evidence: xash3dpp/src/sound/sound.cpp:314,320; include/xash3dpp/private/sound/audio_command.hpp:170; private/sound/mixer.hpp:302; tests/sound/test_sound_topology.cpp:191; implementation-plan.md Chunk 9 Deferred line
- **[L10-forward-fit-obligations-register]** OBL-12-4 — Host frame-loop wiring: Client::init() (non-dedicated only), Client::shutdown(), Client::RunFrame(), Platform::PollEvents(), and the `client::Client client;` member in EngineContext after `server` in declaration order.
  - evidence: xash3dpp/src/host/host.cpp:84,185-186,232,253; include/xash3dpp/host/engine_context.hpp:74,95
- **[L10-forward-fit-obligations-register]** OBL-12-5 — The sound/input link edges must land ALONGSIDE fixing the launcher's null-dependency Host::Main bypass, not instead of it. `xash::Host host; return host.Main(args);` builds HostInitParams with dep pointers intentionally null; the fully-wired path is exercised by one test file, not the shipped binary. New CMake edges without this fix are graph-only.
  - evidence: xash3dpp/src/launcher/main.cpp:90; CORRECTIONS.md confirmed defect; sound/input/imagelib are leaf static libs with zero in-src linkers today
- **[L10-forward-fit-obligations-register]** OBL-12-6 — Vendor a real SDL2/evdev IEventSource backend and a real SDL-backed IWindowControls (MockEventSource and NullWindowControls are the only implementations shipped through Chunk 10, per the ratified Q-23 mock-source stop-line); wire `makehelp`; own the touch/OSK draw calls and the touch editor-chrome list.
  - evidence: xash3dpp/include/xash3dpp/input/event_source.hpp:5; window_controls.hpp:8,48; src/input/keys/commands.cpp:95-99; src/input/touch/touch.cpp:296,337; input.hpp:42,58,146,249,263,290,297-298; private/input/osk_model.hpp:4, touch_model.hpp:3,170,223
- **[L10-forward-fit-obligations-register]** OBL-12-7 — ThreadPriority::Realtime is a stub (logs Warning, runs at Normal) on both backends, awaiting the SDL audio device's real-time scheduling (T_AudioCallback). Also macOS/BSD thread-naming.
  - evidence: xash3dpp/include/xash3dpp/platform/thread.hpp:75; src/platform/win32/thread.cpp:92; src/platform/posix/thread.cpp:98
- **[L10-forward-fit-obligations-register]** OBL-12-8 — Save/client-adjacent deferrals: the `saveshot` preview-image Cbuf hook; pfn_is_map_valid is still the chunk6 ALWAYS-FAIL stub; SV_InactivateClients + SV_FinalMessage changelevel teardown; restart-current-map fallback; live-player client-machinery state; gameinfo.txt aged-count parse; the SV_FindGlobalEntity global-entity registry repoint target; IsValidSave's dead-player/intermission veto; the .bmp thumbnail-eviction hook alongside every age_save_list call; RestoreDecal/LoadClientState live decal application (save provides only the landmark-offset rebase math).
  - evidence: xash3dpp/src/server/lifecycle/save_bridge.cpp:424,755,789,844,888,943,948,1041; include/xash3dpp/private/save/save_directory.hpp:82; private/save/adjacent_transfer.hpp:20,128
- **[L10-forward-fit-obligations-register]** OBL-12-9 — Supply production implementations for IDecalListProvider, IDynamicSoundsProvider, IMusicStateProvider (save's client-state providers) and IMapValidityChecker (pfnGetSaveComment wiring — save_comment()/save_comment_file() have zero production callers today).
  - evidence: xash3dpp/include/xash3dpp/private/save/client_state.hpp:65,80,101; private/save/save_comment.hpp:108
- **[L10-forward-fit-obligations-register]** OBL-12-10 — Server must implement ITrustOracle and wire it into EngineContextInitParams::trust_oracle. It is nullptr in every production path, so the stuffcmd trust gate — which cmd_cvar-boundary.md calls the 'headline G-1 provider, already exists' — is PERMANENTLY DISABLED in production. The in-code marker names chunk6-S9, a chunk that shipped.
  - evidence: xash3dpp/include/xash3dpp/server/server.hpp:84; include/xash3dpp/cmd_cvar/observers.hpp:63
- **[L10-forward-fit-obligations-register]** OBL-12-11 — Cvar-change notification for FCVAR_USERINFO (client) and FCVAR_SERVER|FCVAR_MOVEVARS (server). NOTE THE CROSS-LENS CONFLICT: L11-SUB-2 recommends DELETING ICvarObserver (zero impls, zero call sites, no chunk marker, ~43 lines) and re-adding it from a recorded shape when this consumer lands. If SUB-2 is taken, this obligation becomes 'reconstruct per the shape recorded in cmd_cvar-boundary.md's D3 row'. Either way the obligation survives; only its form changes. This obligation carries ZERO in-code chunk tag — unlike the other 72 markers it is invisible to every mechanical pass.
  - evidence: xash3dpp/include/xash3dpp/cmd_cvar/observers.hpp:41; context.hpp:206; cvar_ops.cpp:13-20,224,379; private/cmd_cvar/context_impl.hpp:45-50; server-boundary.md:179
- **[L10-forward-fit-obligations-register]** OBL-12-12 — Wire the cl_filterstuffcmd gate so ICompatPolicy::is_filterable_exempt gains its caller. FCVAR_FILTERABLE is already set on 24 production cvars with nothing consuming it.
  - evidence: xash3dpp/src/cmd_cvar/cmd_dispatch.cpp:148-157; include/xash3dpp/private/cmd_cvar/compat_policy.hpp:34-36
- **[L10-forward-fit-obligations-register]** OBL-12-13 — Supply a production IBaselineResolver (networking delta) and a Server-side ITrustOracle so the privileged-stuffcmd legacy path can fire at all.
  - evidence: networking delta seam; include/xash3dpp/cmd_cvar/observers.hpp:63
- **[L10-forward-fit-obligations-register]** OBL-12-14 — ABI vendored-ahead surfaces owe their client-side wiring: event_state_t is vendored with the comment that 'the client (Chunk 12) reuses the same layout for its own event ring'; sound_api_t / sound_interface_t / channel_t / rawchan_t are layout-pinned by test but have ZERO production consumers, with HUD_GetSoundInterface negotiation explicitly deferred.
  - evidence: xash3dpp/include/xash3dpp/abi/event_state.hpp:7; include/xash3dpp/abi/sound_api.hpp (whole file, commit 72293dc7); tests/sound/test_sound_api_layout.cpp; sound-boundary.md:64-73
- **[L10-forward-fit-obligations-register]** OBL-12-15 — Reproduce the FIELD_FUNCTION name-mangling passes and fold CL_DisableVisibility() into fullvis (pfn_set_fat_pvs).
  - evidence: xash3dpp/src/server/abi/engine_table.cpp:1440,1869,1893
- **[L10-forward-fit-obligations-register]** OBL-12-16 — Refine calc_fps()'s client branch with gl_vsync / demo-playback checks; it currently assumes gl_vsync==0.
  - evidence: xash3dpp/src/core/clock.cpp:143
- **[L10-forward-fit-obligations-register]** OBL-12-17 — When client work first opens the delta pipeline, execute the snapshot_* → delta_frame_* rename per the L1 vocabulary rule (165 of 308 tree-wide `snapshot` tokens are in server and are wire entity-delta, not P-2). IDENTIFIERS ONLY — field widths, ordering and codec arithmetic are HB-2 fenced.
  - evidence: xash3dpp/src/server/clients/snapshot.cpp (92 occurrences); include/xash3dpp/private/server/snapshot.hpp; snapshot_alloc_baselines/_ring/_signon, snapshot_reset, snapshot_shutdown
- **[L10-forward-fit-obligations-register]** OBL-12-18 — PERMANENT BLOCKER, not a candidate for relaxation: client DLL calls are pinned to T_Main by the non-reentrant cdll_int.h ABI. No client-side thread-assert waiver may be written as class=pending-decision.
  - evidence: xash3dpp/docs/design/threading-model.md:462-467 §8.4
- **[L10-forward-fit-obligations-register]** OBL-12-19 — PLAN DRIFT to resolve at gate time: Chunk 12's Subsystems line claims `client, demo stub, ui stub` and its complexity note says 'defer VGUI/UI to Chunk 13', while Chunk 13 is the renderer and Chunk 14 is 'leaves (demo, ui)'. Three statements, two of which cannot both be right.
  - evidence: xash3dpp/docs/implementation-plan.md:268-276 (Chunk 12) vs :290-296 (Chunk 14)

### 12 (client) — 2

- **[L11-subtraction]** Supply production implementations for IDecalListProvider, IDynamicSoundsProvider, IMusicStateProvider (save/client_state.hpp), IMapValidityChecker (pfnGetSaveComment wiring — save_comment()/save_comment_file() have zero production callers today), IEventSource (input), IBaselineResolver (networking delta), and a Server-side ITrustOracle so the privileged-stuffcmd legacy path can fire at all.
  - evidence: xash3dpp/include/xash3dpp/private/save/client_state.hpp:65,80,101; xash3dpp/include/xash3dpp/private/save/save_comment.hpp:108; xash3dpp/include/xash3dpp/cmd_cvar/observers.hpp:63
- **[L11-subtraction]** Wire the cl_filterstuffcmd gate at cmd_dispatch.cpp:151 so ICompatPolicy::is_filterable_exempt gains its caller; FCVAR_FILTERABLE is already set on 24 production cvars with nothing consuming it.
  - evidence: xash3dpp/include/xash3dpp/private/cmd_cvar/compat_policy.hpp:34-36; xash3dpp/src/cmd_cvar/cmd_dispatch.cpp:148-157

### HB-12 — 1

- **[L11-subtraction]** Replace both xorshift32 stubs with one shared generator matching engine/common/common.c:54-117 (int idum + NTAB=32 shuffle, COM_SetRandomSeed semantics). Until then the two 25-line copies stay independently seeded; consolidating them into a server-private XorShift32 is optional churn, not the fix.
  - evidence: xash3dpp/src/server/abi/engine_table.cpp:1554-1583; xash3dpp/src/server/physics/init_client_move.cpp:278-303

### Chunk 13 (renderer) — 5

- **[L1-publish-primitive]** If RenderFrame is built as the P-2 reference implementation, it must satisfy the shape constraints above rather than inventing a fourth bespoke mechanism — and the HB-5 entry must be updated to stop citing RenderFrame as an existing reference while it is zero code.
  - evidence: grep RenderFrame over xash3dpp/: zero hits outside Documentation/codex/ and .claude/worktrees/ copies
- **[L2-registry-unification]** Land the imagelib key-column conversion (L2-R3) BEFORE the renderer links xash3dpp_imagelib. Today imagelib has test-only linkage and the blast radius is 9 sites inside one target; once the renderer is a second caller of `ImageDecoder::decode` and of any extension-support query, the `handles()` virtual leaks into a second subsystem and the conversion stops being local.
  - evidence: src/content/CMakeLists.txt:62 (`TODO(Chunk 7): PUBLIC xash3dpp_imagelib`), src/content/imagelib/imagelib.cpp:94, include/xash3dpp/private/imagelib/codec.hpp:26
- **[L2-registry-unification]** The backend selector is a NEW dispatch site and the first one built after this lens. It must be born in the canonical shape (see shape_constraints row 1) rather than becoming shape number four. This is a precondition on the renderer boundary spec, which does not exist yet.
  - evidence: src/renderer/CMakeLists.txt:2,7 (four named target APIs); brief: GL-vs-Vulkan is a stated Chunk 13 precondition
- **[L8-thread-model-packet]** The render-thread decision belongs here, gated behind the GL-vs-Vulkan precondition it depends on. §6.5's rule applies: design the RenderFrame ownership boundary first (it is currently zero code), start Chunk 13 as a direct Main-thread call with the struct in place, and move to T_Render only once the data boundary is verified.
  - evidence: xash3dpp/docs/design/threading-model.md:365-372 §6.5; ThreadRole::Render has no use outside src/core/thread_role.cpp:45 and tests/core/test_thread_role.cpp:76
- **[L8-thread-model-packet]** If T_Render is adopted, the cmd_cvar retrofit (all four parts of L8-R4, not just a mutex) and the 8 plain/mixed stats structs become hard preconditions, and HB-5 must be BUILT rather than briefed. Sequence them ahead of the renderer work, not alongside it.
  - evidence: 40 production cvar read sites (sound 17 / server 23 / networking 0); 14 stats structs, 6 atomic / 7 plain / 1 mixed; 13 corpus findings tagged blocks-G3-read

### 13 — renderer — 1

- **[L3-introspection-stats]** imagelib is the renderer's dependency and its Main pin is incidental, resting entirely on two plain counters. L3-R6 should land before Chunk 13 texture work so the decode path's threading contract is settled rather than inherited.
  - evidence: imagelib/imagelib.hpp:30-34; src/content/imagelib/imagelib.cpp increment sites ~77/110/112/117 (F149)

### 13 (renderer) — 1

- **[L11-subtraction]** Supply the production IModelPostProcess implementation and wire content→imagelib. Three sources currently disagree on whether this is done: content-boundary.md marks OQ-3 RESOLVED in one section and deferred-to-13 in another, while CMakeLists.txt:62 and content.hpp:55 still carry a stale TODO(Chunk 7) that makes the obligation ungreppable.
  - evidence: xash3dpp/include/xash3dpp/content/content.hpp:37; xash3dpp/src/content/CMakeLists.txt:62-64; xash3dpp/docs/boundaries/content-boundary.md:424-433

### 13 — 4

- **[L9-renderer-packet]** Adapt xash::imagelib::Image -> legacy rgbdata_t at the renderer seam; imagelib itself never freezes on the legacy struct.
  - evidence: xash3dpp/include/xash3dpp/imagelib/image.hpp:6-8
- **[L9-renderer-packet]** Wire ImageDecoder& into content::InitParams and add the content->imagelib PUBLIC CMake link — currently stale-tagged 'TODO(Chunk 7)' in two of three markers though implementation-plan.md:205 assigns it to Chunk 13; imagelib.decode() has zero production callers anywhere in the tree until this lands.
  - evidence: xash3dpp/include/xash3dpp/content/content.hpp:55; xash3dpp/src/content/CMakeLists.txt:62; xash3dpp/docs/implementation-plan.md:205
- **[L9-renderer-packet]** Validate imagelib's compressed-format passthrough contract (DXT/BC*/KTX2Raw, kept-compressed-for-GPU-decode) against the chosen backend's actual texture-format support — this contract has never been exercised by a real consumer, and the KTX2 codec's own comment already names ref_vk as the intended reader.
  - evidence: xash3dpp/include/xash3dpp/imagelib/pixel_format.hpp:29-43; xash3dpp/src/content/imagelib/codec_ktx2.cpp:13
- **[L9-renderer-packet]** Fix the incidental Main-pin on ImageDecoder::decode() (ImageStats -> atomic) before or during Chunk-13 scaffold; the async texture-decode worker path in threading-model.md §6.4 needs it callable off-Main regardless of backend.
  - evidence: xash3dpp/src/content/imagelib/imagelib.cpp:71-79; xash3dpp/include/xash3dpp/imagelib/imagelib.hpp:30-34; xash3dpp/docs/design/threading-model.md:358-363

### 13 renderer — 12

- **[L10-forward-fit-obligations-register]** OBL-13-1 — The GL-vs-Vulkan-vs-multi-backend decision is a stated Chunk-13 precondition, and Q-10 already scopes it here (not to Chunk 12). Its desktop-vs-mobile sub-choice determines OBL-13-11.
  - evidence: xash3dpp/docs/implementation-plan.md:282,328; decisions-architecture.md:507-508 (Q-10)
- **[L10-forward-fit-obligations-register]** OBL-13-2 — Wire imagelib::ImageDecoder& into content::InitParams and add the content→imagelib (and content→map_loader) PUBLIC CMake link. imagelib.decode() has ZERO production callers anywhere in the tree until this lands. The marker is stale-tagged `TODO(Chunk 7)` in two of three places while implementation-plan.md:205 assigns it to Chunk 13, and content-boundary.md marks OQ-3 RESOLVED in one section and deferred-to-13 in another — three sources, three answers, obligation ungreppable.
  - evidence: xash3dpp/include/xash3dpp/content/content.hpp:55-56; src/content/CMakeLists.txt:62-64; docs/implementation-plan.md:205; content-boundary.md:424-433
- **[L10-forward-fit-obligations-register]** OBL-13-3 — Adapt xash::imagelib::Image → legacy rgbdata_t at the RENDERER seam; imagelib itself never freezes on the legacy struct.
  - evidence: xash3dpp/include/xash3dpp/imagelib/image.hpp:6-8
- **[L10-forward-fit-obligations-register]** OBL-13-4 — Validate imagelib's compressed-format passthrough contract (Dxt1/3/5, Ati2, Bc4-7, Bc7Unorm/Srgb, Ktx2Raw kept compressed for GPU decode) against the chosen backend's actual texture-format support. THIS CONTRACT HAS NEVER BEEN EXERCISED BY A REAL CONSUMER; the KTX2 codec's own comment already names ref_vk as the intended reader.
  - evidence: xash3dpp/include/xash3dpp/imagelib/pixel_format.hpp:29-43; src/content/imagelib/codec_ktx2.cpp:13
- **[L10-forward-fit-obligations-register]** OBL-13-5 — Convert ImageStats's two plain uint64 counters to relaxed atomics and drop the incidental Main-pin from ImageDecoder::decode() (keep init/shutdown Main-pinned for pool lifecycle). Backend-agnostic; safe to land before the backend decision; required by threading-model.md §6.4's async texture-decode path.
  - evidence: xash3dpp/include/xash3dpp/imagelib/imagelib.hpp:30-34; src/content/imagelib/imagelib.cpp:54,61,71-79 and increment sites ~77,110,112,117; threading-model.md:358-363 §6.4
- **[L10-forward-fit-obligations-register]** OBL-13-6 — Become the production implementer of content::IModelPostProcess (the already-resolved OQ-4 injected seam for post-load GPU prep such as VBO building). Do NOT add a second post-load hook.
  - evidence: xash3dpp/include/xash3dpp/content/content.hpp:37
- **[L10-forward-fit-obligations-register]** OBL-13-7 — RenderFrame is HB-5's cited P-2 REFERENCE IMPLEMENTATION and is ZERO CODE (grep over xash3dpp/ returns nothing outside Documentation/codex/ and worktree copies). Either build it to the double-buffered write_idx/read_idx shape already fully specified in threading-model.md §6.3, or stop citing it as an existing reference in the HB-5 entry. Do not invent a fourth bespoke publish mechanism.
  - evidence: xash3dpp/docs/design/threading-model.md §6.3; implementation-plan.md:591-599 (HB-5); grep RenderFrame over xash3dpp/ = zero hits
- **[L10-forward-fit-obligations-register]** OBL-13-8 — ThreadRole::Render is a reserved additive enum slot with ZERO production registrants; Chunk 13 owes the render-thread spawn + register_thread_role(Render), and must only spawn it when the chosen backend's RendererCaps::wants_render_thread is true (GL: false; Vulkan: true). core owes nothing further — the door is already open.
  - evidence: xash3dpp/include/xash3dpp/core/thread_role.hpp:59; src/core/thread_role.cpp:45; decisions-architecture.md:389-390 (Q-6)
- **[L10-forward-fit-obligations-register]** OBL-13-9 — The backend selector is a NEW dispatch site and the first one built after this audit. It must be born as a keyed `constexpr` table {std::string_view key, factory} in the header beside the backend declarations, compared once with utilities::ci_equal — not a per-backend handles()/supported() virtual, not a function-local pointer array. src/renderer/CMakeLists.txt:7 already names four target APIs.
  - evidence: xash3dpp/src/renderer/CMakeLists.txt:2,7; the four existing dispatch shapes: filesystem k_archive_types, k_wad_types, sound k_audio_codecs, imagelib registry[] at imagelib.cpp:94
- **[L10-forward-fit-obligations-register]** OBL-13-10 — IF T_Render is adopted: the cmd_cvar retrofit (all four parts — shared_mutex, a VALUE-RETURNING off-main read API, a permanent Main-only fence on cvar_find/cvar_get_list because pfnCVarGetPointer's escaping cvar_t* is frozen ABI, and the tls_ctx thread_local trap) and the 7 plain + 1 mixed stats structs become HARD preconditions, and HB-5 must be BUILT rather than briefed. Sequence them ahead of renderer work, not alongside it.
  - evidence: xash3dpp/src/cmd_cvar/context.cpp:19 (tls_ctx); cvar_ops.cpp:185,357 (mem_free of the returned string); threading-model.md §8.3; 13 corpus findings tagged blocks-G3-read
- **[L10-forward-fit-obligations-register]** OBL-13-11 — CONDITIONAL on the backend decision naming mobile as a primary target: imagelib's PixelFormat set has no ETC2/ASTC entries and would need them before mobile-native compressed assets can be decoded. Do NOT add speculatively.
  - evidence: xash3dpp/include/xash3dpp/imagelib/pixel_format.hpp:29-43
- **[L10-forward-fit-obligations-register]** OBL-13-12 — SEQUENCING: land imagelib's key-column dispatch conversion (add a key column, delete IImageCodec::handles and its 7 one-line overrides) BEFORE the renderer links xash3dpp_imagelib. Today imagelib has test-only linkage and the blast radius is 9 sites inside one target; once the renderer is a second caller the handles() virtual leaks into a second subsystem and the conversion stops being local.
  - evidence: xash3dpp/src/content/imagelib/imagelib.cpp:94-102; include/xash3dpp/private/imagelib/codec.hpp:26,36-44; overrides at codec_bmp.cpp:306, codec_dds.cpp:394, codec_ktx2.cpp:240, codec_mip.cpp:288, codec_png.cpp:418, codec_tga.cpp:228, codec_wad.cpp:141

### Chunk 14 (demo, ui) — 1

- **[L2-registry-unification]** If demo playback grows a version/format selector, it is a 5th extension-or-magic-keyed dispatch site and triggers this lens's rerun condition before it is written.
  - evidence: src/demo/ contains only CMakeLists.txt — 0-TU skeleton, no boundary spec

### 14 — leaves (demo, ui) — 1

- **[L3-introspection-stats]** Both are 0-TU skeletons with no boundary spec, so q21_scan's silence about them is not health. Whatever stats/introspection they gain must be registered as DiagChannels from the composition root, not by adding parameters to diagnostics_dump — which is the whole point of the span signature in L3-R1.
  - evidence: debug-stats-design.md:359 (the positional sketch this replaces)

### 14 demo/ui — 5

- **[L10-forward-fit-obligations-register]** OBL-14-1 — demo and ui are 0-TU skeletons with NO boundary spec, so q21_scan's silence about them is not health. Both need at least the stub spec of §2a before scaffold.
  - evidence: xash3dpp/src/demo/ contains only a comment-only CMakeLists.txt; same for src/ui/; neither appears in q21_scan's 16-doc denominator
- **[L10-forward-fit-obligations-register]** OBL-14-2 — If demo playback grows a version/format selector it becomes the 5th extension-or-magic-keyed dispatch site and must fire the L2 rerun trigger BEFORE it is written, so it is born in the canonical keyed-table shape rather than becoming shape number five.
  - evidence: four existing shapes: filesystem k_archive_types/k_wad_types, sound k_audio_codecs + the shadowing k_audio_extensions, imagelib's function-local raw pointer array at imagelib.cpp:94
- **[L10-forward-fit-obligations-register]** OBL-14-3 — crc32_block_sequence is an inert declaration awaiting a demo/resource-integrity caller. Span-shape it (std::span`<const std::uint8_t>`, the >60-byte clamp becomes .first()) AT THE MOMENT it gains one, never speculatively.
  - evidence: utilities crc32_block_sequence declaration; Phase-2 verdict: shape-only
- **[L10-forward-fit-obligations-register]** OBL-14-4 — Whatever stats/introspection demo and ui gain must be registered as DiagChannels from the composition root, never by adding parameters to a positional diagnostics_dump. debug-stats-design.md:359's positional sketch is not merely unscalable, it is UNBUILDABLE: host does not link sound/input/content/imagelib, so the positional form demands 4 new link edges while a channel-span form demands zero.
  - evidence: xash3dpp/docs/design/debug-stats-design.md:359; src/host/CMakeLists.txt link list
- **[L10-forward-fit-obligations-register]** OBL-14-5 — Resolve the ui ownership drift before scaffold: Chunk 12 lists `ui stub` in its Subsystems line and says 'defer VGUI/UI to Chunk 13', while Chunk 14 is titled 'leaves (demo, ui)'. Also carries the Chunk-12-deferred touch/OSK drawing.
  - evidence: xash3dpp/docs/implementation-plan.md:268-276 vs :290-296; src/input/touch/touch.cpp:296,337

### ABI-v2 brief (unscheduled, extension-goals.md §5) — 1

- **[L4-state-shape-g2]** Do not start from server-boundary.md's Extension-axes table until L4-R3 lands — three of its rows (P-3 :522, P-5 :526, P-7 :528) are false and the G-2 row (:518) is optimistic about EngineBridge's localisation. Also decide the single/multi-instance question explicitly rather than inheriting it.
  - evidence: CORRECTIONS.md 'Doc claims found FALSE' table; server-boundary.md:518,522,526,528

### Chunk 12/13 — 2

- **[L5-topology-compat]** content->imagelib and content->map_loader PUBLIC links, tagged TODO(Chunk 7) in src/content/CMakeLists.txt:62-64, are actually a Chunk 13 obligation per implementation-plan.md — tag drift, not a missing decision (already tracked by content packs F27/F150).
  - evidence: src/content/CMakeLists.txt:62; implementation-plan.md:205
- **[L5-topology-compat]** sound/input/imagelib link edges must land alongside, not instead of, fixing the launcher's null-dependency Host::Main bypass — otherwise the new edges are graph-only.
  - evidence: launcher/main.cpp:90 (CORRECTIONS.md confirmed defect)

### unowned — LIVE DEFECT (also Chunk-11 gate item 11-G2) — 1

- **[L10-forward-fit-obligations-register]** OBL-X-11 — Four EngineBridge mirrored scalars are read in production and written nowhere. sv_time is the serious one: it drives EdictArena's legacy slot-reuse grace, is frozen at 0.0, and so silently corrupts the 0.5s reuse grace on EVERY game-DLL entity create/free. novis and autoaim_threshold are read but never wired from sv_novis / sv_aim. group_mask/group_op are written but never read (the live copies are move_env->group_mask and links->set_group_op) and should be deleted. No existing gate can see this class — it carries no stub marker, which is how it survived Chunk 6B, a consolidation audit, a close-out audit and 35 modernization packs.
  - evidence: xash3dpp/include/xash3dpp/private/server/engine_bridge.hpp:96; reads at src/server/abi/engine_table.cpp:100,711,1097,2245; only assignment is tests/server/abi/test_engine_table.cpp:145; group_mask/group_op written at engine_table.cpp:2033-2034

### Any chunk adopting an off-main option — 1

- **[L8-thread-model-packet]** Establish real pool-allocation synchronisation before any off-main code pool-allocates. threading-model.md:566 claims 'memory — allocate | Any (pool spinlock) | Today'; no such primitive exists. It is not a live bug only because sound's decoder thread allocates via the system heap (topology.cpp:438), never the pool.
  - evidence: grep -rn 'spinlock|atomic_flag|std::mutex|shared_mutex' over src/memory/, include/xash3dpp/memory/, include/xash3dpp/private/memory/ returns zero matches

### already overdue — 1

- **[L5-topology-compat]** Q-20's own text promises a compliance-scan rule "when the server scaffold lands"; the scaffold has been built and extended through Chunk 10 and the rule still does not exist.
  - evidence: decisions-architecture.md:898-899; server-boundary.md:573; zero matching rule in xash3dpp/tools/xtools/rules.py or compliance_scan.py

### server — 1

- **[L7-determinism-rng]** HB-12's tracked inventory and target shape must reflect legacy's single-generator, function-pointer-aliased reality, not the 'independent streams' premise one amendment round recorded
  - evidence: engine_table.cpp:1556, init_client_move.cpp:281, sv_pmove.c:478-479, cl_pmove.c:769

### sound — 1

- **[L7-determinism-rng]** dsp.cpp's random_long must be added to HB-12's inventory (or explicitly, deliberately excluded with a written reason) rather than silently absent
  - evidence: dsp.cpp:39-48 vs implementation-plan.md:656-663

### unassigned — needs an owner row in implementation-plan.md — 1

- **[L11-subtraction]** Server-side master-list heartbeat: implement IMasterListConfig and call create_master_list_client. The only in-tree pointer is a stale XASH3DPP-STUB(chunk6-S9) at src/server/physics/physics.cpp:1801 naming chunks that already shipped.
  - evidence: xash3dpp/src/server/physics/physics.cpp:1801; xash3dpp/docs/implementation-plan.md:188

### unowned — 5

- **[L10-forward-fit-obligations-register]** OBL-X-2 — HB-12 (one shared deterministic RNG, byte-exact ran1 per engine/common/common.c:54-118) has as its recorded owner the phrase 'a future harmonization session'. Its tracked inventory is also INCOMPLETE: a third stand-in in src/sound/dsp.cpp:39-47 is invisible to the backlog item that exists to unify RNG stand-ins.
  - evidence: xash3dpp/docs/implementation-plan.md:656-663; src/sound/dsp.cpp:39-47 (live-reachable via RoomDsp::profile() at dsp.cpp:734-735 despite the :36 'not load-bearing' comment)
- **[L10-forward-fit-obligations-register]** OBL-X-4 — Networking's netchan pool-allocation TODOs (reliable_buf and per-stream fragment queues from the parent NetworkContext's pool) are tagged Chunk 7, which the 2026-07-04 renumbering reassigned to the content pipeline — now DONE. The target chunk passed without the work landing or being rescheduled, and the mechanical marker scan reports 0 hits for networking.
  - evidence: xash3dpp/src/networking/netchan.cpp:148-149,220,542; implementation-plan.md:193
- **[L10-forward-fit-obligations-register]** OBL-X-7 — cmd_cvar's `exec` and `stuffcmds` built-ins are empty lambdas and cvar_write_variables is a no-op stub, all awaiting the filesystem VFile read/write API and the host's launch-line +commands injection. No chunk owns either dependency.
  - evidence: xash3dpp/src/cmd_cvar/context_init.cpp:182,187; src/cmd_cvar/cvar_ops.cpp:400
- **[L10-forward-fit-obligations-register]** OBL-X-8 — IMapLoaderObserver is the one interface that is neither justified nor scheduled: assign it a chunk (G-4 map-load diagnostics is the natural owner) or delete it. EITHER WAY, server-boundary.md:153's false 'IMapLoaderObserver impl | map_loader FSM' row must be corrected in the same commit — Server does not derive from or implement it anywhere.
  - evidence: xash3dpp/include/xash3dpp/map_loader/map_loader.hpp:51; docs/boundaries/server-boundary.md:153
- **[L10-forward-fit-obligations-register]** OBL-X-9 — HB-5, HB-6 and HB-7 are open DOOR entries with no chunk number. HB-5's answer through Chunk 12 is 'no primitive warranted' under the recommended thread-model default and should be recorded as a DISCHARGE with a Chunk-12/13 re-open gate; HB-6's first buildable slice is the channel-span diagnostics aggregator plus a `stats` console built-in; HB-7 has no chunk-numbered driver at all and must stay shape-only (its alignment lift is NOT gating for G-5 — all three shortlisted runtimes clear the 8-byte bar).
  - evidence: xash3dpp/docs/implementation-plan.md:591-599 (HB-5), :601-609 (HB-6), :610 (HB-7); docs/design/scripting-runtime-brief.md §5.1-5.3

### unowned — LIVE DEFECT — 2

- **[L10-forward-fit-obligations-register]** OBL-X-6 — The shipped launcher never constructs EngineContext. `xash::Host host; return host.Main(args);` builds HostInitParams with 'dep pointers intentionally left null (standalone path)'. The fully-wired path is exercised by ONE test file, not the binary. Any Chunk-12/13 link edge added on top of this is graph-only.
  - evidence: xash3dpp/src/launcher/main.cpp:90; CORRECTIONS.md confirmed defect ('the code is worse than the finding claims')
- **[L10-forward-fit-obligations-register]** OBL-X-10 — snapshot_alloc_ring breaks its own invariant on OOM: writes num_client_entities = count BEFORE the two mem_calloc calls, then returns ok=false without calling free_rings.
  - evidence: xash3dpp/src/server/clients/snapshot.cpp:438,441,444,462

### unowned — LIVE DEFECT, fix rather than own — 1

- **[L10-forward-fit-obligations-register]** OBL-X-5 — get_compat_policy() is unreachable: no header declaration and no caller, so GoldSrcCompatPolicy is never instantiated and EVERY GoldSrc quirk is inert in production. Link-time compat selection (Q-12) does not actually select anything. Both definitions already exist and match; the fix is one header line plus one default at the injection site, landed with a parity test asserting GoldSrcCompatPolicy is selected under XASH_GOLDSRC_COMPAT=1.
  - evidence: xash3dpp/src/cmd_cvar/compat_goldsrc.cpp:102; compat_null.cpp:27; include/xash3dpp/private/cmd_cvar/compat_policy.hpp; src/host/engine_context.cpp:45; null-guarded uses at cvar_ops.cpp:33,105 and cmd_ops.cpp:45

### unowned — OVERDUE — 1

- **[L10-forward-fit-obligations-register]** OBL-X-3 — Q-20's own text promises a compliance-scan rule confining raw entvars_t/edict_t access 'when the server scaffold lands'. The scaffold landed in Chunk 6 and was extended through Chunk 10; the rule does not exist in xtools/rules.py or compliance_scan.py, yet server-boundary.md:516 ASSERTS that it guards the confinement.
  - evidence: decisions-architecture.md:898-899; server-boundary.md:516,573; zero matching rule in xash3dpp/tools/xtools/rules.py

### unowned — needs an owner row or an explicit closure — 1

- **[L10-forward-fit-obligations-register]** OBL-X-1 — Server-side master-list heartbeat: implement IMasterListConfig and call create_master_list_client. The ONLY in-tree pointer is a stale XASH3DPP-STUB(chunk6-S9) marker naming chunks that already shipped.
  - evidence: xash3dpp/src/server/physics/physics.cpp:1801; docs/implementation-plan.md:188

## Open decisions

### L1-publish-primitive

- **Is HB-5 discharged by a rule + shape-constraint register, or does it stay open until a primitive exists?**
  - options: (a) rule now + gate the primitive [recommended]; (b) keep HB-5 fully open; (c) close HB-5 outright and re-open at Chunk 13
  - recommendation: Discharge the analysis half now (rule + constraints, L1-R1/R2), and re-scope the remaining half as gated on Chunk 12 (L1-R3). A backlog item whose answer is 'no, and here is why, and here is what to do instead' is discharged, not abandoned.
  - owner: architecture / decision register
- **Wire Sound::channels_snapshot() to a production consumer, or leave the tree's only cross-thread structured publisher consumer-less?**
  - options: (a) record only [recommended]; (b) wire s_show to a console dump (effort S, consumer = the already-registered cvar); (c) implement save::IDynamicSoundsProvider over it (in-tree named interface, zero impls — but it wants a span borrow, not a published read)
  - recommendation: Leave it and record it. The obvious wiring — the s_show cvar is registered at sound.cpp:463 and never read — would be building a debug overlay to justify a mechanism, which is the anti-gold-plating failure inverted. Record in sound-boundary.md that the mechanism is test-exercised only, so the next audit does not read it as dead code.
  - owner: sound owner / Chunk 14
- **Keep or delete Cvar::generation, given it has two publishers and zero consumers?**
  - options: (a) keep + boundary row [recommended]; (b) delete generation and both fetch_adds (prefers-deletion, but discards the one ready-made publish edge)
  - recommendation: Keep and document (L1-R5). Cost is 4 bytes per Cvar plus one release fetch_add per write; it is the cheapest correct change-epoch already in the tree and the natural first HB-5 test case. But the boundary row must say 'no consumer' explicitly, or it will be miscounted as a working mechanism — the campaign brief itself already miscounted it in the opposite direction.
  - owner: cmd_cvar owner

### L2-registry-unification

- **Is case-INSENSITIVE extension dispatch the parity-correct behaviour, and is the existing test that asserts the opposite therefore wrong?**
  - options: (a) Ratify `ci_equal` everywhere and amend tests/sound/test_sound_codec.cpp:142 (`CHECK(!wav_codec().handles("WAV"))`) — which encodes the current, divergent behaviour as if it were a requirement. (b) Keep case-sensitive dispatch and accept that xash3dpp silently skips `FOO.PAK` where legacy mounts it, and document the divergence. (c) Split the difference: ci_equal in filesystem only (where the divergence is provable), case-sensitive in codec tables.
  - recommendation: (a). Legacy compares with `Q_stricmp` at all three sites — filesystem.c:3441, img_main.c:278/361/546/570, snd_utils.c:454 — so case-insensitive IS the contract, and the sound test is pinning an implementation detail rather than a behaviour. (c) is the worst option: it reintroduces the per-subsystem divergence this lens exists to remove. Note this is a behaviour change inside a closed (Chunk 9) subsystem, so it needs an explicit owner sign-off rather than riding in on a cleanup commit.
  - owner: sound + filesystem subsystem owners; ratify in the decision register only if the sound owner disputes that the test is wrong
- **Should the canonical shape ship a shared lookup helper (e.g. `utilities::find_by_key(span, key)`) or stay a per-site 4-line loop under a written convention?**
  - options: (a) Convention only — each site keeps its own `for` loop, four lines, no new API. (b) Add a small constexpr template to utilities (L0, everyone links it) serving 4 call sites today.
  - recommendation: (a), and revisit at the 5th table. Four call sites of a 4-line loop do not justify a new generic API in a tree that has deliberately zero `concept`s and zero CRTP, and the anti-gold-plating rule points the same way. The convention is what prevents drift; the helper would only prevent typing. Re-evaluate when the renderer backend table makes it five.
  - owner: HB-13 author
- **Does L2-R5's `/w14062` go in as part of HB-13 or as its own HOUSE entry?**
  - options: (a) HB-13 item D — it is the enforcement half of the same 'stop writing obligations as prose' argument. (b) A separate HOUSE backlog entry, since it is a tree-wide build-flag change touching a file none of the three conversions touch and could surface unrelated diagnostics.
  - recommendation: (b) if the first build after adding the flag surfaces more than ~2 sites; (a) otherwise. Decide by running one build with the flag before choosing — the cost of the decision is a single configure+build, and 52 of 57 switches already carry a `default:`.
  - owner: HB-13 author

### L3-introspection-stats

- **Where do the aggregator types and the assembly live?**
  - options: (a) all in `core`; (b) types in `core`, assembly + `stats` command registration in `host`; (c) a new `diagnostics` library as debug-stats-design.md §6.3 sketches.
  - recommendation: (b). `core` is the only common ancestor of all 17 targets (every subsystem already links it), so `DiagSink`/`DiagChannel`/`diagnostics_dump` there cost zero new link edges. Assembly belongs in `host` because that is the composition root that owns the objects and already links cmd_cvar. (c) is premature — §6.3's diagnostics library is for the UDP/TCP external channel, and building the lib now to hold three POD structs adds a target for zero benefit.
  - owner: HB-6 brief author
- **Does `EntityView` count as one of HB-6's P-4 channels, as implementation-plan.md:601-609 asserts?**
  - options: (a) yes, keep the doc as written; (b) no — correct the plan text and record that server's entity channel does not yet exist.
  - recommendation: (b). EntityView is in a private header, is a live accessor facade rather than a snapshot, and cannot be reached by any frontend. Naming a private Q-20 discipline device as a public introspection channel is how a Chunk 12 author ends up making it public 'because the plan said so'. Correct the plan; record the missing entity-list channel as SC-7 rather than building it.
  - owner: HB-6 brief author
- **Should the `stats` command be gated (XASH_STATS / XASH_DEBUG_*) or always present?**
  - options: (a) always present, printing whatever tiers are compiled in; (b) gated like the existing `hashstats` (XASH_DEBUG_CVARS).
  - recommendation: (a). §5's rule is 'always measure, gate the output' and §6.5 says do not gate counter increments on a runtime bool — but the command itself is the query side, which §6.5 and extension-goals P-4's stats note both say is the runtime part. `hashstats` being XASH_DEBUG_CVARS-gated is itself flagged as questionable in §6.4's own harmonization table. Always-present also means the command is exercised in release CI.
  - owner: HB-6 brief author
- **Do subsystems export their own `emit` adapter, or does host write all 14?**
  - options: (a) each subsystem ships `void <subsys>_diag_emit(const void*, const DiagSink&) noexcept` in its own lib; (b) host writes captureless lambdas converted to fn-ptr for everything it owns.
  - recommendation: (a) for subsystems host already links, (b) not at all. (a) keeps the formatting next to the fields it names (so adding a counter and adding its line are one edit), and costs each subsystem only a dependency on core/diagnostics.hpp — which every subsystem already has via core. It explicitly does NOT require any subsystem to link a diagnostics target, which was the constraint. For subsystems host does not link (sound, input, content, imagelib) the adapter still ships with the subsystem and is simply not registered until a root that owns them exists.
  - owner: HB-6 brief author

### L4-state-shape-g2

- **Cvar storage ownership: does the engine registry adopt the DLL's `cvar_t` as live storage (indirection), keep a copy with write-through, or stay split with fall-through lookup only?**
  - options: (a) fall-through lookup only [S, one file, closes the read gap, two stores remain, no serverinfo/collision-check parity]; (b) indirect storage — registry node gains `CvarAbi *storage` defaulting to `&abi` [M, touches cmd_cvar's offset-0 layout contract, full legacy semantics, one store]; (c) copy + write-through mirror [S, but re-creates the hand-synced-mirror pattern that produced the sv_time defect]
  - recommendation: (b). It is the only option that restores legacy's single-list semantics (BaseCmd collision check, Cvar_UpdateInfo serverinfo propagation, cvarlist visibility) and the only one that scales to Chunk 12's client DLL without a third chain. Take (a) as an immediate stopgap only if Chunk 11 is already in flight — it closes the crash-class read gap for one file's worth of edits.
  - owner: the decision register (Q-* entry), owner to be assigned at promotion
- **The P-5 door rule says existing signatures 'migrate in Chunk 6B (the scheduled full-retrofit wave)'. Chunk 6B shipped and 121 whole-`ServerRuntime` signatures remain. Is the rule now false, or is a wave owed?**
  - options: (i) declare the rule new-code-only and amend extension-goals.md:236-238 to say so; (ii) schedule a bounded wave covering the 30 pure leaves only, and record the 21-function corridor as explicitly deferred with its ordering constraint; (iii) schedule the full retrofit
  - recommendation: (ii). It is the only option that is both honest about the measurement and bounded — the 30 leaves have no ordering constraint, and the corridor sits in physics/ next to Chunk 11's frozen pm_shared where a signature wave would collide with parity work. (iii) is the gold-plating trap; (i) leaves a false claim standing in the doc.
  - owner: implementation-plan.md / the Q-22 owner
- **Is 'more than one ServerRuntime can exist' a G-2 requirement, or only 'the ABI is multithreading-suitable'?**
  - options: (A) single-instance forever — then L4-R6 buys only testability and shim clarity; (B) instance-parametric — then L4-R6 is a hard precondition and `s_fatpvs`/`s_fatphs` plus the two RNG statics must move into the context too
  - recommendation: Answer it in the ABI-v2 brief, not before — but note that L4-R6 is worth doing under (A) anyway, so it is not blocked on the answer. extension-goals.md:39 says G-2 is 'a multithreading-suitable engine<->game interface', which does not by itself imply multi-instance; do not let the brief assume it silently.
  - owner: the ABI-v2 design brief (extension-goals.md §5, currently unwritten)
- **Should `stub_scan` (or compliance_scan) gain a 'member read but never written in production' rule?**
  - options: (i) yes, as a stub_scan rule; (ii) yes, as a compliance_scan candidate-WARNING; (iii) no — treat the four EngineBridge fields as a one-off
  - recommendation: (i) or (ii). HB-11's inventory explicitly depends on inline stub markers, and this defect class carries none — four fields survived Chunk 6B, a consolidation audit, a close-out audit and 35 modernization packs precisely because nothing looks for them. The rule is cheap to express over the compile DB and would have caught sv_time, novis and autoaim_threshold in one run.
  - owner: xash3dpp/tools/ owner

### L11-subtraction

- **Is ICvarObserver deleted (SUB-2) or given a chunk number and kept as a recorded door?**
  - options: (a) Delete now, ~43 lines plus two empty hot-path loops, shape recorded for cheap reconstruction. (b) Keep and attach a chunk number — but the only candidate consumers (Chunk-12 client userinfo, G-1 MCP) are the same ones that have not materialised across four chunks.
  - recommendation: Delete. It is the only construct in the tree with zero impls, zero call sites AND no chunk marker; if it were a real door someone would have marked it as one during Chunks 7-10. Reconstruction from the recorded shape is under an hour.
  - owner: decisions-architecture register owner
- **Is IMapLoaderObserver assigned a chunk or deleted?**
  - options: (a) Assign G-4 / map-load diagnostics ownership and add the door row. (b) Delete the interface, attach_observer/detach_observer, the storage, and the 2 test doubles.
  - recommendation: Whichever is chosen, server-boundary.md:153's false implementation claim must be fixed in the same commit — that half is not optional. Between the two, deletion is the honest default: an observer with no observers, no callers and no chunk after four chunks of map_loader work has no demonstrated demand.
  - owner: map_loader boundary owner
- **Is the GoldSrc compat policy wired (SUB-4) or deleted?**
  - options: (a) Wire: two lines, activates the HL25 redirect / CMD_OVERRIDABLE / filter-exemption quirks and CHANGES observable behaviour by design. (b) Delete 141 lines of currently-unreachable code and drop the Q-12/D12 claim.
  - recommendation: Wire it, with a parity test in the same commit. The code transcribes legacy behaviour with per-table legacy citations; deleting it turns a wiring omission into a silent parity regression in a parity-first fork. This is the one place in this lens where addition beats subtraction, and the test that decides it — 'does this have a legacy-derived behavioural contract, or only a future-consumer contract?' — should be recorded as the general rule.
  - owner: Q-12 owner / server boundary owner
- **Do the six empty src/{client,demo,physics,renderer,ui,world} directories get deleted?**
  - options: (a) Delete all six comment-only CMakeLists (~60 lines, 6 directories) — CMake never reads them, implementation-plan.md already owns the chunk intent and CLAUDE.md says the plan wins on any contradiction. (b) Keep as intent markers. (c) Delete only src/world and src/physics, whose stated scope already shipped inside src/server/world/ and src/server/physics/.
  - recommendation: (a). They are documentation wearing build-system clothing, they inflate the tree from 17 real targets to 23 directories, and two of the six are factually false about what is unbuilt. If placement intent for Chunk 13's renderer matters, it belongs in implementation-plan.md where the plan can be revised, not in a file CMake will never read.
  - owner: implementation-plan owner
- **Should IEventSource, IDecalListProvider, IDynamicSoundsProvider, IMusicStateProvider and IMapValidityChecker — all Chunk-12-blocked — be re-examined before Chunk 12 starts, given that Chunk 12's thread-model decision is a stated precondition?**
  - options: (a) Record the door rows now (SUB-5) and revisit at Chunk 12 kickoff. (b) Defer all five until the thread-model decision lands.
  - recommendation: (a). The door rows are documentation of what exists and cost nothing; the thread-model decision may change their signatures but not their existence, and having them recorded is what will make the Chunk-12 precondition review tractable.
  - owner: Chunk 12 owner

### L8-thread-model-packet

- **THE PACKET — Chunk 12 thread-model precondition (implementation-plan.md:270): does network I/O or rendering run off-main? Forced choice among (a) stay Main-only through Chunk 12; (b) move NetIO off-main; (c) move rendering off-main; (d) both.**
  - options: (a) MAIN-ONLY — 0 of 22 decision-coupled waivers discharged, 0 new asserts owed, cmd_cvar retrofit not forced, memory pool sync not forced, 0 plain stats structs forced atomic, HB-2 wire kernels untouched, test matrix unchanged, does not block Chunk 12. The 13 blocks-G3-read + 3 needs-publish-idiom findings stay parked as classification-only; F18/F29/F139/F149 remain documentation findings. || (b) NETIO OFF-MAIN — discharges all 22 waivers (transport asserts T_NetIO, delta asserts sim); owes ~22 new asserts in a subsystem that has ZERO today; does NOT force the cmd_cvar retrofit (networking reads zero cvars — verified: 40 production cvar reads tree-wide, sound 17 / server 23 / networking 0); NetworkingStats+DeltaStats already fully atomic so no stats work; forces memory pool sync if the transport pool-allocates (netchan.cpp:216 sets impl_->pool and nothing ever reads it — currently inert); re-tiers F87 (non-const IProtocolDriverRegistry::resolve forcing a mutable singleton) to urgent and F79 (HB-5) up; moots F127 (resolve_blocking's NetIO-only contract gains a real role); RIDERS: loopback stays T_Main forever (§7.2) so singleplayer/multiplayer transport affinity diverges permanently, and the HB-2 fenced wire bit-codec / delta widths / LZSS / OOB magic then execute under two topologies so the byte-exact witness must be re-derived per topology; neither of §7.4's own two triggers (async DNS/HTTP; 64+ players) has fired. || (c) RENDER OFF-MAIN — discharges 0 waivers; forces the full cmd_cvar retrofit (per-frame gl_* reads) INCLUDING the value-returning API, not just a mutex; forces memory pool sync; forces >=4 plain stats structs atomic (ImageStats, ClockStats, HostStats, ContentStats) and makes the whole 13-finding blocks-G3-read set urgent, re-tiering F94 (Clock::stats returns a live const& into a non-atomic struct) from High to Blocker; requires HB-5 to be BUILT not just briefed; and is NOT DECIDABLE AT CHUNK 12 — renderer is a 0-TU skeleton with no boundary spec, ThreadRole::Render has no use outside thread_role.cpp:45's name table and test_thread_role.cpp:76, §6's named reference implementation RenderFrame is zero code, and Chunk 13 carries its own undecided GL-vs-Vulkan precondition that this decision depends on. || (d) BOTH — the union of (b) and (c) costs, plus a 3-thread topology in a tree whose only multi-threaded subsystem to date required a dedicated campaign (S9.0-S9.8), a console-scripted byte-exact witness, and caught one cross-thread use-after-free and one orchestrator-introduced data race in its own gates.
  - recommendation: RECOMMENDED DEFAULT: (a) stay Main-only through Chunk 12, and convert the NetIO question from an open decision into a TRIGGERED one by citing threading-model.md §7.4's two existing named triggers (async DNS/HTTP consumer appears; player count reaches 64+). Rationale: (1) the binding design doc already answers this — §3.5 states verbatim 'Chunks 8/10/11/12 (save, input, physics, client) — Model B unchanged. No new threads.' and §3.7 defers T_NetIO 'until dedicated server load demonstrates a need'; the implementation plan simply does not cite them. (2) No consumer forces (b): §7.4's triggers are unfired, networking reads zero cvars, and its 22 waivers are already written to survive either answer ('role unasserted UNTIL the NetIO thread is split out'). (3) The tree already solved the only real cross-thread configuration problem without any of this — sound reads its 17 cvars on Main and marshals them as MixConfigSnapshot PODs over the MPSC AudioCommand queue (dsp.cpp:74-88, audio_command.cpp:263-275), zero cmd_cvar changes, witness-tested. (4) (c) is undecidable at Chunk 12 by construction, so bundling it guarantees the precondition never closes. (5) Per extension-goals.md §6, (b)/(c)/(d) are speculative infrastructure with no day-one consumer; (a) is the only option that is not. || EXPLICIT FALLBACK — IF STILL UNDECIDED WHEN CHUNK 12 STARTS, PROCEED ON THIS ASSUMPTION: Model B unchanged, all client work on T_Main, T_NetIO and T_Render both deferred. This is not a coin-flip default — it is what threading-model.md §3.5 already binds, and it is the only assumption under which every one of the 99 waivers, all 13 blocks-G3-read findings, and both zero-TU-skeleton subsystems remain valid without a single line changing. Choosing (a) costs nothing later: options (b)/(c)/(d) all remain reachable, because §7.3's abstraction boundary commitment is already met (recvfrom/sendto confined to context.cpp:224/259 behind IPlatformSockets) and §6.5's 'design the ownership boundary first, threading is mechanical' rule applies to the renderer. || GATES TO ATTACH TO (a): the four false doc rows in L8-R3/R4 must be corrected regardless of the answer, because they are what would make a later (b)/(c)/(d) unsafe.
  - owner: Chunk 12 planning owner (implementation-plan.md:270 gate) — with the render half reassigned to the Chunk 13 owner alongside GL-vs-Vulkan
- **SUB-DECISION (adjudicated, stated for the record): unify assert_main_thread() onto assert_thread_role(Main), or document two deliberate notions and fix the docs?**
  - options: UNIFY — 4 line edits in src/platform/{win32,posix}/{console,crash}.cpp, drop capture_main_thread() from {win32,posix}/sys.cpp:53/57, delete include/xash3dpp/private/core/assert_main.hpp, correct thread_role.hpp:21-23 and threading-model.md:11,81-83. || DOCUMENT TWO NOTIONS — keep the lazily-capturing helper as a deliberate 'pre-role-registration' guard, correct the three doc sites to stop calling it a wrapper, and record why two mechanisms exist.
  - recommendation: UNIFY. The 'two deliberate notions' reading fails on four counts. (1) Scale: 4 call sites, all in one subsystem — not a mechanism worth a second concept. (2) Two of the four are already dead: console::read_line has zero production callers (host.cpp:244 carries an unwired OQ-9 TODO), so the live surface is crash::install_handler x2. (3) The lazy semantics are strictly WORSE for the one live site — assert_main.hpp:50-53 short-circuits on id == std::thread::id{}, so the guard on a process-wide POSIX sigaction install silently no-ops during exactly the startup window it exists to protect, whereas register_thread_role(Main) is the first statement of main() (launcher/main.cpp:64) and arms assert_thread_role strictly earlier. (4) The doc-drift liability has already fired three times in three files, and thread_role.hpp:21-23 even cites the wrong path (private/platform/ vs the actual private/core/). The unification is a net DELETION of a header and a mechanism, which the audit brief explicitly prefers. Residual cost, stated honestly: after unification crash::install_handler aborts in any host that never calls register_thread_role — that is the intended behaviour, and the shipped launcher always registers. NOTE the code is the honest party here: assert_main.hpp documents its own no-op window accurately at lines 44-53; it is the three OTHER documents that assert a rewrite that never happened.
  - owner: L8-R2 (HB-new)

### L7-determinism-rng

- **Should sound/dsp.cpp's random_long be folded into HB-12's byte-exact port scope, or explicitly scoped OUT (VOX/DSP profiling may not need bit-exact COM_RandomLong parity)?**
  - options: (a) fold in -- one port, three call sites migrated; (b) explicitly exclude with a written reason once dsp.cpp's live-reachability is resolved (F73)
  - recommendation: Resolve F73's dead-code contradiction first (is dsp.cpp:734-735 really reachable in production, or only via a debug/profile command not present in shipped builds); that answer determines whether (a) or (b) is correct. Do not guess.
  - owner: sound subsystem owner + whoever picks up HB-12
- **How should a Chunk-11 determinism regression test handle a single shared, stateful RNG instance being called from both the client-predicted and server-authoritative PM_Move paths inside one test process, given a single continuously-advancing generator leaves the two paths observing different states at comparison time?**
  - options: (a) two instances, each explicitly reseeded to the same value immediately before each compared call; (b) a documented, test-enforced call-order contract; (c) some other test-harness-specific isolation
  - recommendation: No recommendation -- this is a test-design decision that depends on how the Chunk-11 harness structures the client/server comparison, not something this lens has visibility into. Flagging it now so it is a deliberate choice, not a flaky-test surprise discovered mid-Chunk-11.
  - owner: whoever writes the Chunk-11 determinism regression test

### L9-renderer-packet

- **Renderer backend: GL-compat, Vulkan-first, or abstracted multi-backend (implementation-plan.md:282,328 Chunk-13 precondition)?**
  - options: GL-compat: broadest HW/driver reach, simplest thread model (no T_Render, RenderFrame optional). Vulkan-first: modern explicit API, makes T_Render + RenderFrame load-bearing day one, raises an unresolved ETC2/ASTC mobile-format question if mobile is in scope. Abstracted multi-backend: highest effort — one RenderFrame/plugin-descriptor shape must serve both GL's synchronous submit_frame and Vulkan's threaded one, and imagelib's PixelFormat set must cover the union of both backends' native compressed formats.
  - recommendation: Engine-structure considerations only — this audit takes no position on graphics-API merits, driver maturity, or mobile-reach tradeoffs. What the structure supports: Chunk 12 does not need this decision (Q-6/Q-10 already decouple it); the imagelib decode()-Main-pin fix (L9-2) is safe to do before the decision since it's backend-agnostic; RenderFrame's double-buffer implementation and any ETC2/ASTC codec addition should wait for the decision (and its desktop-vs-mobile sub-choice) since building either speculatively has no named consumer yet, per the anti-gold-plating rule.
  - owner: renderer chunk owner (whoever runs /plan-implementation for Chunk 13)

### L10-forward-fit-obligations-register

- **L10-R4 wants q21_scan's denominator to be xtools.subsystems() (the 22 directories under xash3dpp/src). L11-SUB-1(m) and L11's open decision want the six empty directories and their comment-only CMakeLists deleted. These cannot both happen as written.**
  - options: (a) Delete the six comment-only CMakeLists.txt (CMake genuinely never reads them — they are absent from xash3dpp/CMakeLists.txt's add_subdirectory list) but KEEP the six directories, each holding a one-line README.md pointing at its boundary spec. (b) Delete directories and CMakeLists both, and derive q21_scan's denominator by parsing the `**Subsystems**:` lines from implementation-plan.md's chunk entries. (c) Keep everything as-is and take neither recommendation.
  - recommendation: (b), with (a) as the low-risk fallback. L11 is right that six comment-only CMakeLists CMake never reads are documentation wearing build-system clothing, and right that two of the six (physics, world) are factually FALSE about what is unbuilt — the work shipped inside src/server/physics/ and src/server/world/. But a directory list is a weak authority for a coverage denominator regardless: it would also miss a planned subsystem that has no directory yet, which is precisely the demo/ui case at Chunk 14. implementation-plan.md is the authority CLAUDE.md names, it already enumerates subsystems per chunk, and parsing it makes the denominator delete-proof. Cost is ~15 lines of parsing versus one function call. Choose (a) only if the parsing turns out to be fragile against the plan's prose formatting — verify with one run before committing to (b).
  - owner: Whoever executes the tooling wave — the two recommendations must be scheduled together or one silently undoes the other.
- **L11-SUB-2 recommends deleting ICvarObserver outright (zero implementations of any kind, zero call sites, ~43 lines, no in-code chunk marker after four chunks). OBL-12-11 records Chunk 12 as owing a registration for FCVAR_USERINFO propagation. Delete-and-reconstruct, or keep-and-record-as-a-door?**
  - options: (a) Delete per SUB-2, record the exact replacement shape in cmd_cvar-boundary.md's D3 row and the commit message, and rewrite OBL-12-11 as 'reconstruct per the recorded shape'. (b) Keep it and add a chunk-12-numbered door row per SUB-5, matching the treatment of the other 8 scheduled doors.
  - recommendation: (a). The distinguishing test SUB-2 applies is the right one and this lens's register independently confirms its premise: of the tree's zero-implementation interfaces, ICvarObserver is the ONLY one with zero impls, zero call sites AND no in-code chunk tag — its obligation exists solely in boundary-doc prose (D3 and server-boundary.md:179), which is exactly why it is invisible to every mechanical pass. Note the asymmetry with the sibling decision: get_compat_policy() must be WIRED not deleted, because it transcribes legacy behaviour with per-table citations, so deleting it converts a wiring omission into a silent parity regression. The general rule worth recording is SUB-4's: does this construct have a legacy-derived BEHAVIOURAL contract, or only a future-consumer contract? ICvarObserver has only the latter.
  - owner: cmd_cvar owner + whoever gates Chunk 12
- **Should the forward-obligations register live in implementation-plan.md's per-chunk entries (L10-R1) or as its own document under docs/audits/ or docs/design/?**
  - options: (a) In implementation-plan.md's chunk entries. (b) A standalone docs/forward-obligations.md, cross-linked from each chunk entry. (c) In each subsystem's boundary spec `## Forward obligations` section only.
  - recommendation: (a), with (c) as a SECONDARY mirror and (b) rejected outright. The evidence against (b) is the audit's own premise: 10 of 13 existing modernization reports are stale by three whole chunks, and their content was not the problem. A standalone register is document #14 by construction, and cross-links do not change what a chunk-start prompt actually opens. (c) is a genuinely useful mirror — it is where a subsystem author looks — but it cannot be primary, because Chunk 12's obligations live in nine OTHER subsystems' specs, so the client author reading client-boundary.md would see almost none of them. Only the plan aggregates by chunk, which is the axis the gate needs. Accept the duplication between (a) and (c) and let obligations_scan keep them consistent.
  - owner: Whoever executes L10-R1
- **The two decision packets: is the render-thread question decided at Chunk 12 (as implementation-plan.md:270 currently bundles it) or moved to Chunk 13 alongside the backend choice it depends on?**
  - options: (a) Split: NetIO half closes at Chunk 12 by citing threading-model.md §3.5/§3.7; render half moves to Chunk 13 next to the GL-vs-Vulkan precondition. (b) Keep them bundled and decide both before Chunk 12. (c) Move both to Chunk 13.
  - recommendation: (a), decisively. Q-6 (decisions-architecture.md:389-390) already makes the render-thread spawn a FUNCTION of the backend's declared RendererCaps::wants_render_thread, and Q-10 (:507-508) already scopes the backend policy to 'before Chunk 13'. So the render half is undecidable at Chunk 12 by construction, and bundling it guarantees the precondition never closes — which is its observable state today. (c) is wrong because the NetIO half is decidable NOW and its answer is already written down in a binding design doc the plan simply never cites; leaving it open costs a chunk gate for nothing. (b) is the status quo that produced an unclosable precondition. Note the downstream payoff: under (a)'s NetIO answer (Model B unchanged), HB-5's verdict through Chunk 12 becomes a clean 'no primitive warranted, here is why, here is the shape when one is' — a discharge rather than an item that reads as overdue in the backlog forever.
  - owner: Whoever gates Chunk 12
- **Six of the eight mechanizable re-run triggers are blocked behind tool fixes, two of which (the MUTATOR_NAMES \w* regex, prod_impls counting) are correctness bugs whose current WRONG output is already quoted as fact in shipped boundary docs. Land the tooling wave first, or land the doc corrections first?**
  - options: (a) Tooling wave first, then re-derive every affected doc figure from the fixed scans. (b) Doc corrections first from this audit's hand-verified numbers, tooling wave after. (c) Both in one wave.
  - recommendation: (a), with one carve-out. The doc figures are derived quantities; correcting them by hand from an audit snapshot means they are wrong again at the next commit and a third party has no way to tell which of the two numbers is current. Fix the scanner, then regenerate — the corrections become reproducible rather than asserted. The carve-out is the class of doc claims that are not derived numbers but FALSE STATEMENTS ABOUT CODE (server-boundary.md:153's phantom IMapLoaderObserver impl, :528's 'Q-22 RAII classes' where no destructor exists, :522's 'the one file-scope mutable global', core-boundary.md:230's 'by-value snapshot' that returns const&). Those should be fixed immediately and independently — no scanner will ever produce them, and each is currently load-bearing for someone's future design. Also heed CORRECTIONS' scope caveat: the regex fix is one line but surfaces ~55 new candidate findings needing adjudication; land the fix and the adjudications as separate commits.
  - owner: Whoever executes L10-R6
- **Chunk 11 is next and its gate is five items. Is 11-G2 (fix EngineBridge::sv_time) genuinely a Chunk-11 gate item, or an independent defect fix that need not block?**
  - options: (a) Gate item — Chunk 11 must not start on top of it. (b) Independent defect fix, scheduled immediately but not blocking. (c) Fold into the general L4-R1 mirrored-scalar pass.
  - recommendation: (b), scheduled to land BEFORE Chunk 11 but not formally gating. The defect is real and serious — sv_time is frozen at 0.0 and drives EdictArena's legacy 0.5s slot-reuse grace on every game-DLL entity create/free, so every such call currently takes the wrong branch — but it is a server/ABI-shim defect, not a physics one, and making it a physics gate misattributes ownership in a register whose whole value is accurate attribution. The practical answer is the same either way (fix it now, it is an S), and it should ride with the novis/autoaim_threshold wiring and the group_mask/group_op deletion as one commit plus one regression test freeing an edict at t>2.0 and asserting the grace is honoured. Record it as OBL-X-11 with a note that Chunk 11 is the first chunk to lean hard on this path.
  - owner: Chunk 11's gatekeeper
