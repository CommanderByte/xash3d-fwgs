# Server Migration TODO

This TODO tracks the active server-side engine migration lane opened by Phase
52. Keep it focused on near-term work; broader server rewrites should remain in
the main phase tracker until they are selected.

## Phase 52: Boundary Audit

- [x] Audit `engine/server/` file ownership and coupling.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`.
- [x] Identify compatibility surfaces that must remain stable during early
  migration.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`.
- [x] Define the first server compatibility-layer shape.
  Evidence: `Documentation/codex/modern/engine/server-migration-guide.md`.
- [x] Rank near-term migration candidates.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`.

## Phase 53: Server Filter Pilot

- [ ] Capture focused baseline behavior for existing `sv_filter.c` tests and
  command/file surfaces.
- [ ] Add modern tests for IP filter inclusion, removal, active/expired rules,
  and config/human formatting.
- [ ] Add modern tests for ID filter prefix matching and expiration.
- [ ] Implement target-neutral filter value types and lists under
  `src/engine/server`.
- [ ] Add a legacy adapter that supplies current time and translates legacy
  filter records without moving command handlers yet.
- [ ] Route the smallest safe part of `sv_filter.c` through the adapter.
- [ ] Run focused tests, `.\waf.bat build --alltests`, and a runtime smoke.

## Phase 54: Server Query Response Builder

- [ ] Capture byte-level baseline payloads for details, rules, and players.
- [ ] Add tests for password-protected player-list suppression and protected
  cvar value masking.
- [ ] Implement `SourceQuerySnapshot` and response builder under
  `src/engine/server`.
- [ ] Keep `NET_SendPacket` and live server-state reads in the legacy adapter.
- [ ] Run focused golden tests, `.\waf.bat build --alltests`, and a runtime
  smoke.

## Deferred Server Items

- [ ] Server event logging service after console/log sink ownership is clearer.
- [ ] Declarative server command registration after filter/query pilots.
- [ ] User-agent/input-device policy extraction after connection tests exist.
- [ ] Save/restore migration after binary compatibility fixtures exist.
- [ ] Game DLL bridge migration after explicit ABI and licensing review.
- [ ] Physics/world migration after movement and trace fixtures exist.
