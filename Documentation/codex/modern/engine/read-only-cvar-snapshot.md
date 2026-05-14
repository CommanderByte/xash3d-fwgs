# Read-Only Cvar Snapshot

Phase 112 adds a tiny read-only cvar snapshot helper. This is not a cvar
registry migration. It only gives modern helpers a stable way to receive plain
boolean, numeric, integer, and string values without depending on `convar_t`.

## Current Cvar Ownership

The legacy cvar system remains owned by:

- `engine/common/cvar.c` for registration, lookup, mutation, filtering,
  callbacks, and config/archive persistence;
- `engine/common/base_cmd.c` for the shared command/alias/cvar name table;
- the existing `Cvar_*` public and game DLL callback surfaces;
- server cvar declarations and registration in `engine/server/sv_main.c`.

Phase 112 does not move any of those responsibilities.

## Server Read-Only Cvar Candidates

The audit found several server helpers that already feed plain cvar values into
modern policy helpers:

| Area | Legacy owner | Cvar shape | Migration note |
| --- | --- | --- | --- |
| User-agent input-device policy | `SV_ProcessUserAgent()` in `sv_main.c` | five booleans | First Phase 112 route-through. Low risk because the modern user-agent helper already accepts plain booleans. |
| Userinfo penalty | `SV_ShouldUpdateUserinfo()` in `sv_client.c` | booleans and numbers | Good follow-up, but it also mutates per-client penalty state, so keep it behind the existing client-policy adapter for now. |
| Client rate limits | `SV_CheckUpdateRate()` and `SV_CheckRate()` in `sv_client.c` | numbers | Good follow-up once rate-limit snapshots are grouped with client policy. |
| Download/customization policy | `SV_DownloadFile_f()` and custom-resource paths in `sv_client.c` / `sv_custom.c` | booleans, strings, size limits | Needs resource and filesystem ownership context, so defer. |
| Source query and NetAPI info | `sv_query.c`, `sv_client.c` | booleans, strings, cvar iteration | Cvar iteration is registry-owned and should not move in this phase. |
| Logging | `sv_log.c` | booleans and cvar iteration | Defer until console/logging sinks are revisited. |
| Save/restore and movevars | `sv_save.c`, `sv_main.c` | numeric and string snapshots | Tied to save format and movevars synchronization, so defer. |

## Modern Helper

The helper lives in:

- `src/include/engine/cvar_snapshot.hpp`
- `src/engine/cvar_snapshot.cpp`

`ReadOnlyCvarSnapshot` stores:

- `exists`;
- copied text;
- numeric value;
- integer value;
- boolean value.

The helper intentionally does not know cvar names, flags, callbacks, defaults,
or registry links.

## Legacy Adapter

The first C adapter lives in:

- `engine/server/server_cvar_snapshot_adapter.h`
- `engine/server/server_cvar_snapshot_adapter.cpp`

It adapts `struct convar_s` into a plain snapshot or directly returns boolean,
numeric, integer, and string values for legacy C callers. The adapter keeps the
legacy string pointer for C callers and uses the modern helper for normalized
numeric, integer, and boolean interpretation.

## Phase 112 Route-Through

`SV_ProcessUserAgent()` now reads these cvars through the adapter:

- `sv_allow_noinputdevices`
- `sv_allow_touch`
- `sv_allow_mouse`
- `sv_allow_joystick`
- `sv_allow_vr`

The surrounding behavior remains unchanged:

- `Info_ValueForKey()` still parses user-agent strings;
- `SV_CheckID()` still owns ban checks;
- `SV_RejectConnection()` still owns rejection side effects;
- cvar registration and mutation stay legacy-owned.

## Validation

Phase validation used:

- focused target: `test_engine_cvar_snapshot`;
- engine target: `xash`;
- full tests: 117/117;
- runtime smoke:
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit`;
- first frame: 0.516 seconds.
