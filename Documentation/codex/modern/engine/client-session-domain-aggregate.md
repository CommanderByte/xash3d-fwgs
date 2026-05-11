# Client Session Domain Aggregate

Phase 154 strengthens the client/session aggregate before adapter shrink. The
goal is to prove that the small client-facing helpers compose as a domain while
keeping packet and runtime ownership in the legacy server.

## Covered Helpers

`tests/engine/client_session_domain.cpp` now covers the following client/session
areas together:

- connectionless command classification;
- challenge window admission and connection rejection formatting;
- user-agent validation and rejection messages;
- remote admin command authentication and argument assembly;
- client slot population, free-slot search, and master-server update choices;
- client userinfo flags, userinfo update penalties, rate, and update interval
  limits;
- client command dispatch routes, including builtin, enttools, game DLL,
  fullupdate, and throttle paths;
- timeout planning for active spawned clients;
- Source query details and player-list privacy policy;
- NetAPI legacy server info formatting.

## Legacy-Owned Boundaries

Phase 154 does not move live client/session runtime ownership. These remain in
legacy server files:

- packet reads, token parsing, and source address handling;
- `netchan_t`, reliable/unreliable buffers, fragments, and transfers;
- downloads, uploads, voice, movement packet parsing, and PMove callbacks;
- live `sv_client_t` mutation and entity ownership;
- query packet send calls and socket ownership;
- game DLL packet forwarding and callback ordering.

## Consolidation Result

The aggregate supports a small adapter shrink in Phase 155. It does not justify
a broad `sv_client.c` replacement facade yet: the helpers are still separate
policy/value functions and the risky runtime state is deliberately legacy-owned.
