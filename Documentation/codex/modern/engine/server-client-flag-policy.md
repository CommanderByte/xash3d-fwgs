# Server Client Flag Policy

Phase 109 starts turning private `FCL_*` bit checks into named client capability
predicates. This is intentionally not a broad macro replacement.

## Legacy Flag Buckets

The server client flags have several different owners:

| Bucket | Flags | Primary users |
| --- | --- | --- |
| Fake/human identity | `FCL_FAKECLIENT` | query payloads, filters, commands, movement, game DLL callbacks, frame loops, connection cleanup |
| Spectator proxy | `FCL_HLTV_PROXY` | frame spectator datagrams, multicast, custom resources, HLTV connection paths |
| Prediction capabilities | `FCL_PREDICT_MOVEMENT`, `FCL_LOCAL_WEAPONS`, `FCL_LAG_COMPENSATION` | userinfo parsing, player movement, client data, game DLL event playback |
| Frame message state | `FCL_SKIP_NET_MESSAGE`, `FCL_SEND_NET_MESSAGE`, `FCL_RESEND_USERINFO`, `FCL_RESEND_MOVEVARS` | `sv_frame.c` and userinfo/movevars resend paths |
| Resource/consistency state | `FCL_SEND_RESOURCES`, `FCL_FORCE_UNMODIFIED` | download, customization, and consistency paths |

Because these flags encode several unrelated concerns, the safe pattern is to
add predicates by owner instead of replacing every `FBitSet()` call.

## Modern Contract

`client_policy` now exposes:

- `ClientFlagSnapshot`: decoded private client flags for tests and adapter
  inputs.
- `ClientIsFakeClient()`
- `ClientIsHltvProxy()`
- `ClientUsesLocalWeapons()`
- `ClientPredictsMovement()`
- `ClientUsesLagCompensation()`
- `ClientShouldAppearInHumanQueries()`

These helpers live beside the existing userinfo/rate policy because prediction,
fake-client, and query-visible human status are client capability decisions,
not separate global bit utilities.

## Route-Through Scope

Phase 109 routes one low-risk owner:

- `sv_query.c`: source-query player rows now use
  `SV_ClientPolicy_ShouldAppearInHumanQueries()` to decide whether a player is
  a human query participant or a fake client with `-1.0` duration.

The following remain legacy-owned:

- flag mutation in `sv_client.c`, `sv_frame.c`, and `sv_custom.c`;
- frame resend/send/skip state machines;
- HLTV connection flow and spectator datagram routing;
- game DLL event playback admission;
- local-weapons, prediction, and lag-compensation side effects beyond the
  already-routed userinfo parsing plan.

## Follow-Up

The next safe route candidates are owner-specific:

- source-query and filter fake-client predicates;
- spectator proxy predicates in frame/customization helpers;
- local-weapons predicates inside the later event playback phase;
- resend/send/skip predicates only when frame message ownership is revisited.
