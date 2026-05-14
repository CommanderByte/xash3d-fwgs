# Server Migration Guide

This guide translates the Phase 52 server boundary audit into a migration style
for future server work.

## Direction

The long-term goal is a modular server runtime where internals are ordinary C++
objects and services, while compatibility quirks remain explicit at the edges.
For now, the edge is still the existing C server surface:

- `SV_*` functions;
- `Log_*` functions;
- `server.h` structs and globals;
- game DLL and physics callback tables;
- legacy command/cvar names;
- save, config, log, and protocol formats.

Modern server code should therefore start as private implementation behind
legacy adapters, not as a public replacement API.

## Ownership Split

Use this split for new server migration slices:

| Layer | Location | Owns |
| --- | --- | --- |
| Pure modern logic | `src/engine/server/` | Target-neutral policy, builders, snapshots, plain value types, tests. |
| Private modern headers | `src/include/engine/server/` | C++ contracts used by modern tests and adapters. |
| Legacy glue | `engine/server/*_adapter.cpp` when legacy headers are needed | Translation from `server.h`, `common.h`, `sv`, `svs`, `svgame`, `Cmd_*`, `Cvar_*`, `FS_*`, and `NET_*`. |
| Legacy C surface | Existing `engine/server/*.c` | Stable `SV_*` and `Log_*` entry points until a later ABI decision. |

Avoid including `server.h` from pure modern code. If a helper needs live server
state, define a snapshot or narrow plain-data input and let the adapter build it.

## Namespaces

Use `xash::engine::server` for target-neutral server internals.

Suggested first sub-areas:

- `xash::engine::server::filters`
- `xash::engine::server::query`
- `xash::engine::server::logging`
- `xash::engine::server::commands`

Do not expose these namespaces through game DLL, client DLL, renderer DLL, or
filesystem ABI surfaces.

## First Class Sketches

### Filters

Good first classes:

- `IpFilterRule`
- `IpFilterList`
- `IdFilterRule`
- `IdFilterList`
- `FilterClock`
- `FilterFormatter`

The lists should not know about `Cmd_*`, `Con_*`, `FS_*`, `svs.clients`, or
`SV_DropClient`. They should answer questions such as:

- does this address match an active rule?
- does this ID match an active rule?
- which rules should be removed by this remove request?
- which rules are permanent and should be written to config?
- what text should represent this rule for human or config output?

The adapter owns command parsing, printing, file writes, client iteration, and
time-source conversion.

### Source Queries

Good first classes:

- `SourceQuerySnapshot`
- `SourceQueryPlayer`
- `SourceQueryRule`
- `SourceQueryBuilder`
- `SourceQueryPlatform`

The builder should produce bytes into a caller-provided buffer or modern byte
writer. It should not call `NET_SendPacket`, read cvars, walk `svs.clients`, or
call `svgame.dllFuncs` directly.

### Server Logging

Server event logging remains a later slice. When it starts, separate:

- event record construction;
- timestamp formatting;
- file path selection;
- remote UDP log address state;
- console echoing;
- file/packet sinks.

Do not fold server event logs into the system console backend. They may share a
future sink interface, but the service has different semantics.

## Compatibility Layer Rules

- Keep public C names and prototypes stable until an explicit ABI phase changes
  them.
- Preserve command names and registration timing.
- Preserve cvar names, aliases, flags, and default values.
- Preserve `banned.cfg`, `listip.cfg`, server logs, savegame files, and packet
  payloads byte-for-byte unless a task explicitly records a behavior change.
- Keep `sv`, `svs`, and `svgame` as adapter-owned state while migrating leaf
  policies.
- Never let exceptions cross the C boundary.
- Prefer explicit status/result returns in adapter-facing code.
- Do not make pure modern server code depend on renderer, launcher, filesystem
  module internals, or platform-specific console APIs.

## Acceptance Standard

A server slice is ready to route legacy code through modern code when:

1. legacy behavior is documented or covered by tests;
2. modern unit tests cover normal, edge, and compatibility cases;
3. the legacy adapter changes only one behavior surface at a time;
4. `.\waf.bat build --alltests` passes;
5. a runtime smoke is run if the slice affects startup, networking, file output,
   client connection, command registration, or packet payloads.

## Near-Term Phases

Phase 53 should migrate filter policy first.

Phase 54 should migrate source-query payload building second.

Only after those pass should we decide whether to continue through server
logging, command registration, user-agent policy, or another server leaf.
