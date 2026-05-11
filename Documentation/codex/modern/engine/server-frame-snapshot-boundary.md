# Server Frame Snapshot Boundary

Phase 129 audits `engine/server/sv_frame.c`, the live owner of server frame
snapshots and per-client datagram emission. This file is closer to the network
wire than most previous server slices, so broad route-through would be risky
without stronger packet-entity fixtures.

## Legacy Owners

| Area | Legacy ownership reason |
| --- | --- |
| Packet entity selection | `SV_AddEntitiesToPacket()` calls the game DLL `pfnSetupVisibility()` and `pfnAddToFullPack()`, walks every edict, recurses through portal cameras, mutates `sv.hostflags`, updates `cl->viewentity[]`, and fills a local `sv_ents_t` buffer. |
| Packet entity storage | `SV_WriteEntitiesToClient()` sorts accepted entities, writes them into the global circular `svs.packet_entities` buffer, updates `svs.next_client_entities`, and mutates the current `client_frame_t`. |
| Delta entity serialization | `SV_EmitPacketEntities()` chooses full versus delta packet commands, checks stale delta buffers, looks up old/new entity states in `svs.packet_entities`, selects baselines, calls `MSG_WriteDeltaEntity()`, and queries live edict removal state. |
| Baseline selection | `SV_FindBestBaseline()` calls `Delta_TestBaseline()`, reads `svs.packet_entities`, `svs.static_entities`, and `sv.time`, and returns offsets understood by the wire encoder. |
| Event emission | `SV_EmitEvents()` mutates queued event records, maps event entity indexes into the current frame, clears transient event args, and serializes `svc_event`. Phase 111 already owns only the emit-count clamp. |
| Ping emission | `SV_EmitPings()` iterates live clients, calls `SV_GetPlayerStats()`, and writes bit-packed ping/loss rows. |
| Clientdata | `SV_WriteClientdataToMessage()` mutates `client_frame_t`, calls game DLL `pfnUpdateClientData()` and `pfnGetWeaponData()`, consumes `fixangle`, and writes client/weapon deltas. |
| Datagram send loop | `SV_UpdateToReliableMessages()` and `SV_SendClientMessages()` own netchan buffers, resend flags, overflow drops, `Netchan_*` calls, and frame scheduling. Phase 73-era datagram helpers already own some pure gates. |
| Inactive clients | `SV_SkipUpdates()` and `SV_InactivateClients()` mutate client flags, states, customization lists, physinfo, and message buffers during level transitions. |

## Existing Modern Coverage

The frame file is not untouched. Current modern helpers already cover:

- `server_frame_datagram`: reliable/unreliable/spectator datagram copy,
  fragment, ignore, overflow-clear, resend, and send-loop gate decisions.
- `server_event_playback_policy`: queued event emit-count clamping.
- `server_visibility_constraints`: portal viewentity capacity.
- `server_message_envelope` and payload writers: reusable service-message
  and text-message byte layouts outside the packet-entity path.

These helpers are useful, but they do not own entity selection, frame storage,
or delta serialization.

## Fixture Feasibility

| Candidate | Fixture shape | Feasibility | Recommendation |
| --- | --- | --- | --- |
| Client send-loop gates | Plain client state, flags, times, and byte counts. | Already covered by `server_frame_datagram`. | Keep extending only when new gates appear. |
| Queued event emit count | Plain event count and queue capacity. | Already covered by `server_event_playback_policy`. | Keep event queue mutation legacy-owned. |
| Portal viewentity capacity | Plain current count and capacity. | Already covered by `server_visibility_constraints`. | Keep `edict_t` storage legacy-owned. |
| Packet entity delta cursor | Sorted old/new entity numbers, old-frame freshness, and delta availability. | Feasible with plain fixtures if it only returns match/add/remove decisions and header mode. | Best Phase 130 helper candidate. |
| Baseline selection | Entity states plus `Delta_TestBaseline()` behavior. | Needs delta table fixtures or a mockable scorer before route-through. | Defer until a delta scorer abstraction exists. |
| Entity selection visibility | Edict snapshots, PVS/PHS masks, group filters, `pfnAddToFullPack()`, portal recursion. | Too coupled to game DLL callbacks and world state. | Defer until visibility/fullpack fixtures exist. |
| Clientdata and weapondata | Client edict state, previous frame data, game DLL update callbacks, local weapon flags. | Too coupled to game DLL callbacks. | Defer to a game DLL/clientdata phase. |
| Ping rows | Client stats and spawned-state scan. | Testable, but low value because it is mostly wire writing and live stats lookup. | Defer unless a packet writer abstraction is introduced. |
| Inactive-client transition | Client states, fake-client drops, customization cleanup, changelevel preservation, buffer clears. | Could be policy-tested later, but live side effects dominate. | Better fit for a later level-transition phase. |

## Phase 130 Recommendation

Use Phase 130 for a small packet-entity delta planning helper, not a full
snapshot builder.

The helper should be target-neutral and should consume plain facts such as:

- old entity number or end marker;
- new entity number or end marker;
- whether a valid old delta frame is available;
- whether the old frame's circular packet entities have rolled off.

It should return only decisions such as:

- emit full packet header;
- emit delta packet header with sequence;
- emit add-from-baseline;
- emit delta-from-old;
- emit remove-from-old;
- finish packet entity stream.

It must not own:

- `svs.packet_entities` storage;
- `client_frame_t` mutation;
- `MSG_BeginServerCmd()`, `MSG_WriteDeltaEntity()`, or terminator writes;
- `SV_FindBestBaseline()` or `Delta_TestBaseline()`;
- edict removal checks and forced remove decisions;
- game DLL visibility callbacks.

If Phase 130 finds that even this cursor planner makes the legacy code harder
to read, it should stop at tests and documentation. The main value is building
fixture coverage around packet-entity ordering before we touch the real
snapshot builder.

## Validation

Phase 129 is documentation-only. No runtime code changed, so no build or smoke
test was required for this phase.
