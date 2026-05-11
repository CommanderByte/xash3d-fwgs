# Client Transfer, Voice, And Admin Split

Phase 126 checks the non-session parts of `sv_client.c` that were intentionally
left behind after the client slot helper.

## Surface Review

| Area | Current helper coverage | Phase 126 decision |
| --- | --- | --- |
| Downloads | `server_download_policy` covers download admission and sidecar decisions. | Do not move more here now; filesystem probes, HPAK reads, fragments, and failure writes still dominate. |
| Client resource lists | `server_upload_queue` covers descriptor validity, timing, upload-limit, and batch actions. | Defer list parsing/lifetime to the resource-transfer domain. |
| Voice relay | `server_voice_relay` covers payload limits, input gates, physics result gates, recipient decisions, and payload writing. | No new helper needed; message reads, physics callbacks, recipient iteration, and datagram mutation remain legacy-owned. |
| Cvar query responses | No focused helper yet. | Defer as a game DLL/client-query bridge because the useful behavior is callback dispatch plus reporting. |
| Remote admin | Existing code mixed rcon enable/password checks with command-string reconstruction, redirects, and command execution. | Extract only auth action and quoted command reconstruction into `remote_admin_command`. |

## Remote Admin Helper

`remote_admin_command` owns only plain-value policy:

- disabled or empty-password rcon requests are ignored;
- enabled rcon with a bad or missing supplied password is rejected;
- matching password accepts execution;
- accepted command arguments are reconstructed as the legacy quoted argument
  stream, preserving the trailing space after each argument.

`sv_client.c` still owns:

- packet parsing and `Cmd_Argv()` lifetime;
- logging the rcon request and bad-password report;
- redirect buffer setup and flushing;
- `Cmd_ExecuteString()`;
- packet sends through the redirect target.

This keeps the router/admin behavior testable without claiming the console or
network sink ownership too early.

## Follow-Up

The next time this area is touched, prefer one of these domains instead of
adding more one-off adapters:

- resource-transfer aggregate for download and client resource-list parsing;
- game DLL client-query bridge for cvar-value callbacks;
- console/router work for rcon redirects and output sinks.
