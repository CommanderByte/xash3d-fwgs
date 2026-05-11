# Server Adapter Shrink Pass

Phase: 142

## Purpose

This pass inventories repeated glue in `engine/server/*adapter*` and decides
whether any of it should collapse into shared adapter helpers now.

The rule for this phase is conservative: add or reuse shared helpers only when
the helper has one clear domain. Do not merge unrelated adapters just because
they look mechanically similar.

## Inventory

### Bit-Buffer Write Results

Status: already centralized.

`engine/server/server_message_adapter_shared.hpp` owns:

- `MakeNetworkBitBuffer()`, which clamps negative bit positions and sizes
  before constructing the modern `NetworkBitBuffer`;
- `MakeWriteResult<T>()`, which maps `tellBit()` and overflow status into the
  legacy C result struct shape.

Current users include customization, resource, service, sound, spawn-handshake,
static, text, userinfo, and voice relay message adapters. This is a good shared
helper because every caller is a server message writer and the legacy result
shape is intentionally uniform.

No new helper is needed here.

### Resource Snapshots

Status: already centralized.

`engine/server/resource_adapter_shared.{hpp,cpp}` owns:

- `resourcetype_t` to/from modern `ResourceType`;
- `resource_t` to modern `ResourceDescriptor`;
- resource array snapshots for download/catalog decisions;
- derived rows for resource-list and customization message payloads.

This is a good shared helper because every caller is in the resource-transfer
domain and the adapter semantics are tied to the legacy `resource_t` layout.

No new helper is needed here.

### Message Recipient Facts

Status: keep local adapter shapes for now.

Modern code has a reusable `ServerMessageRecipientFacts` aggregate in
`server_message_envelope`, plus builders for multicast, event playback, and
voice recipient requests. That is useful for tests and future grouped
messaging work.

The live C adapters still expose different legacy shapes:

- multicast takes positional facts from `SV_Multicast()`;
- event playback takes `sv_event_recipient_input_t`;
- voice relay takes sender/recipient indices, listener masks, payload size,
  and datagram room.

Those differences mirror existing call sites. Collapsing them into one shared
C adapter struct would either create a mega-struct with fields most callers do
not own or force broader legacy call-site churn. That is not a useful shrink.

No new helper is added here.

### Plain Enum And Boolean Conversions

Status: keep local to the owning adapter.

Many adapters contain small `ToLegacy*()`, `ToModern*()`, or
`static_assert()` blocks. Examples include game DLL entity lifecycle, client
info, resource policy, connectionless classification, map validation, command
lifecycle, packet-entity cursors, timeout actions, physics routing, and world
link child masks.

These look repetitive, but their meaning is domain-specific:

- most conversions map one modern enum to one public C enum declared beside
  the adapter;
- static asserts protect specific protocol, ABI, or compatibility constants;
- a generic enum-conversion helper would hide the compatibility contract rather
  than clarify it.

No cross-domain enum helper is added.

## Shrink Decision

No adapter code was changed in this phase.

The two clear helper domains, bit-buffer write results and resource snapshots,
already exist and are tested. Message-recipient facts and enum conversions
should stay explicit until a later grouped-domain route-through creates a real
shared owner.

This is a useful result even though it does not reduce file count: it prevents
the adapter layer from gaining a broad, vague utility header that would become
another dependency magnet.

## Later Candidates

Potential future cleanup should be domain-specific:

- a grouped messaging adapter pass can revisit recipient facts after multicast,
  event playback, voice, and message session have a single legacy boundary;
- a grouped game DLL bridge adapter pass can revisit repeated callback-table
  enum/static-assert blocks after `sv_game.c` call sites shrink further;
- resource adapters can continue using `resource_adapter_shared` as the single
  resource-transfer bridge.

## Validation

This phase is documentation-only. Validation is `git diff --check` plus
`scripts/phase-status.ps1 -PhaseNumber 142`.
