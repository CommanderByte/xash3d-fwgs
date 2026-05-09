# Legacy Engine Notes

This folder documents the current engine architecture before modernization.
It focuses on real ownership, coupling, global state, and migration risk.

## Documents

- [common-audit.md](common-audit.md) audits `engine/` with a deeper pass over
  `engine/common/`.
- [command-cvar-baseline.md](command-cvar-baseline.md) documents the current
  command/cvar ownership, lifecycle, tests, and migration seams.
- [info-string-baseline.md](info-string-baseline.md) documents the migrated
  `Info_*` compatibility rules, quirks, and test coverage.
