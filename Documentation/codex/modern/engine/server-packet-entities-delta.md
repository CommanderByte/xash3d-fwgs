# Server Packet Entity Delta Planner

Phase 130 adds a narrow helper behind `SV_EmitPacketEntities()` in
`engine/server/sv_frame.c`. This is not a snapshot builder. It is a small
cursor and header planner used while the legacy code keeps ownership of packet
entity storage and wire writes.

## Modern Helper

The helper lives in:

- `src/include/engine/server/server_packet_entities_delta.hpp`
- `src/engine/server/server_packet_entities_delta.cpp`

It owns two pure decisions:

1. Packet-entity header mode:
   - full packet when there is no delta sequence;
   - full packet plus stale-delta warning when the old frame has rolled out of
     the circular `svs.packet_entities` buffer;
   - delta packet when the old frame is still usable.
2. Old/new sorted entity cursor steps:
   - finish when both sides are exhausted;
   - delta from old state when entity numbers match;
   - add from baseline when the new entity number is lower;
   - remove from old state when the old entity number is lower.

The C adapter lives in:

- `engine/server/server_packet_entities_delta_adapter.h`
- `engine/server/server_packet_entities_delta_adapter.cpp`

## Legacy Ownership Preserved

`sv_frame.c` still owns:

- selecting visible entities through `SV_AddEntitiesToPacket()`;
- sorting the accepted `entity_state_t` list;
- writing into `svs.packet_entities`;
- mutating `client_frame_t::first_entity` and `client_frame_t::num_entities`;
- looking up old/new `entity_state_t` pointers from the circular buffer;
- choosing baselines with `SV_FindBestBaseline()` and instanced baseline rules;
- calling `MSG_BeginServerCmd()`, `MSG_WriteDeltaEntity()`, and terminator
  writes;
- checking whether old edicts were removed or marked `FL_KILLME`;
- printing stale-delta diagnostics.

That split keeps Phase 130 useful without moving `entity_state_t`, `edict_t`,
`client_frame_t`, delta tables, or network buffers into the modern helper.

## Compatibility Notes

- The stale-frame condition preserves the old `<=` boundary:
  `old_first_entity <= next_client_entities - num_client_entities`.
- A stale delta request still emits a full `svc_packetentities` header and
  writes the same warning from legacy code.
- Cursor decisions preserve the original sorted merge behavior:
  equal numbers advance both cursors, lower new numbers add, and lower old
  numbers remove.
- The helper receives the legacy end marker as an input so the C side keeps
  ownership of `MAX_ENTNUMBER`.

## Test Coverage

`tests/engine/server_packet_entities_delta.cpp` covers:

- full-packet header when no delta sequence exists;
- fresh delta header with previous-frame reuse;
- stale delta fallback at and below the legacy boundary;
- finish, match, add, and remove cursor actions;
- missing-new and missing-old side behavior.

## Validation

Phase 130 validation:

- `.\waf.bat build --targets=test_engine_server_packet_entities_delta` passed.
- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 126/126 tests.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.514 seconds and stopped with reason `command`.
