# Server Runtime Configuration Boundary

Phase 127 audits `engine/server/sv_main.c`, which is one of the live runtime
owners for the dedicated/listen server. The goal is not to move the whole
server loop at once. The safe boundary is to extract plain policy decisions
while leaving cvars, packets, master servers, shutdown, and frame ordering in
the legacy runtime.

## Legacy Owners

`sv_main.c` still owns these side-effect-heavy areas:

| Area | Legacy ownership reason |
| --- | --- |
| Cvar registration | `SV_Init()` registers global server cvars and command handlers. Registration order, archive flags, and callbacks are part of the engine runtime. |
| Movevars | `SV_UpdateMovevars()` reads many cvars, mutates `sv_zmax` through `Cvar_DirectSet()`, updates `svgame.movevars`, and emits movevar deltas. |
| Packet reads | `SV_ReadPackets()` owns netchan state, client message parsing, fragment reads, reconnect handling, and connectionless packet dispatch. |
| Server frame | `Host_ServerFrame()` sequences timers, packets, world simulation, client messages, master heartbeats, and sleeping. |
| Master heartbeat | `SV_AddToMaster()` and related heartbeat/shutdown paths own packet sends and master-server address validation. |
| Shutdown | `SV_FinalMessage()` and `SV_Shutdown()` own final network messages, client drops, physics shutdown, game DLL shutdown, log flushes, and cvar resets. |

These areas should stay behind the legacy C surface until a broader runtime
object exists. Modern helpers may consume plain snapshots from them, but should
not own live mutation yet.

## Selected Seam

The timeout loop in `SV_CheckTimeouts()` had a small policy core:

- count active real players for pause release;
- free zombie clients once their reuse delay is over;
- drop connecting/spawning clients after `sv_connect_timeout`;
- optionally ban connected clients when `sv_connect_timeout_ban` is enabled;
- drop spawned clients after `sv_timeout`;
- skip timeout decisions for fake clients and local addresses.

Phase 127 moved only that decision into
`src/engine/server/client/server_timeout_policy.cpp`. The legacy adapter builds a
plain request from `sv_client_t`, cvar values, entity flags, and computed drop
points, then applies the returned plan by mutating client state or calling
`SV_DropTimedOutClient()`.

## Compatibility Notes

- Active-player counting still happens before any timeout action, matching the
  old loop. A client dropped during the loop can therefore keep the pause from
  being released until the next frame.
- Fake clients still skip timeout actions. The modern request builder also
  avoids the old skipped local-address probe for fake clients.
- The helper intentionally does not read cvars. `sv_timeout`,
  `sv_connect_timeout`, `sv_connect_timeout_ban`, `svs.maxclients`, and
  `sv.paused` remain legacy-owned values passed into the helper.
- Local-address detection remains legacy-owned because it depends on network
  address types and platform/socket state.

## Validation

Phase 127 validation:

- `.\waf.bat build --targets=test_engine_server_timeout_policy` passed.
- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 124/124 tests.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.511 seconds and stopped with reason `command`.
