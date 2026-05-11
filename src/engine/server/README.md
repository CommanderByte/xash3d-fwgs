# Modern Server Internals

This folder contains target-neutral C++ helpers extracted from
`engine/server/`. The live server runtime still belongs to the legacy C files;
code here should describe plain values, policies, payload builders, and fixture
models that can be tested without owning `sv`, `svs`, `svgame`, packet
buffers, filesystem mutation, console output, or game DLL ABI layout.

## Domains

- `resources/`: resource identity, catalogs, manifests, downloads, uploads,
  consistency, hot resources, and reslist policy.
- `messaging/`: server message envelopes, payload writers, recipient policy,
  voice relay, multicast, event playback, frame datagram, and packet-entity
  cursor planning.
- `game_dll/`: game DLL ABI metadata, load policy, entity lifecycle, entity
  parsing, message sessions, user-message registration, resources, payloads,
  visibility/trace, movement, output, changelevel, and string-pool
  compatibility.
- `client/`: client admission/session helpers, command dispatch, userinfo,
  challenge/rejection, connectionless classification, remote admin, timeout,
  and user-agent policy.
- flat files: shared constraints, runtime helpers, save fixtures, world/PMove
  policies, source-query builders, and NetAPI builders that have not yet moved
  into a grouped domain.

## Boundaries

Modern helpers should avoid including `server.h`. Legacy adapters in
`engine/server/` translate live structs, globals, cvars, commands, packet
buffers, and side effects into plain snapshots before calling this layer.

Keep these legacy-owned until a dedicated fixture-backed phase says otherwise:

- `edict_t`, `sv_client_t`, `server_t`, `server_static_t`, and `svgame`
  storage/lifetime;
- `sizebuf_t`, netchan sends, signon buffers, and raw `MSG_*` mutation;
- game DLL and physics extension ABI publication order;
- filesystem/HPAK/save stream mutation;
- exact world traces, PMove callbacks, physent population, and touch replay;
- console output, command registration, and cvar mutation.

## Near-Term Shape

The existing grouped domains should stay. The remaining flat files can move
later into:

- `shared/` for server-wide limits and constraints;
- `runtime/` for server shell, command, filter, and log policy;
- `save/` for save/restore fixtures and value objects;
- `world/` for world, trace, movement, physics, and PMove policies.

Prefer cohesive domain modules over one tiny file per branch condition. A new
file should introduce a real concept, not just a new noun for a legacy `if`.
