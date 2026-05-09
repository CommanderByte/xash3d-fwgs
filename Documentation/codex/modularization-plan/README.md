# Modularization Plan

This folder is the working plan for moving the engine toward a more modular
C++ internal architecture while preserving the C-compatible ABI surfaces that
make existing Half-Life and GoldSrc content work.

The companion feasibility document explains why an incremental migration is the
right shape. This folder turns that direction into phases, decision points, and
subsystem-level notes.

## Documents

- [roadmap.md](roadmap.md) outlines the proposed migration phases, acceptance
  criteria, and early work items.
- [filesystem-pilot.md](filesystem-pilot.md) is the first subsystem deep dive
  and proposed pilot plan.
- [boundary-filesystem.md](boundary-filesystem.md) records the filesystem
  compatibility boundaries that refactors must preserve.
- [filesystem-modern-design.md](filesystem-modern-design.md) records the Phase
  3 filesystem design decisions for backend adapters, errors, registries, and
  source layout.
- [filesystem-debug-utilities.md](filesystem-debug-utilities.md) records the
  Phase 4 debug command and machine-readable output design.
- [cross-cutting-utilities.md](cross-cutting-utilities.md) records reusable
  modernization utilities for errors, logging, serialization, registries, and
  thread-readiness.
- [subsystem-template.md](subsystem-template.md) is the template for future
  subsystem plans.

## Planning Rules

- Preserve public C ABI boundaries unless a future fork decision explicitly
  drops compatibility.
- Keep each migration step buildable and manually smoke-testable.
- Add tests or a repeatable verification recipe before moving behavior.
- Prefer internal wrappers and facades before moving storage or changing
  function signatures.
- Record compatibility-sensitive quirks instead of "cleaning" them away.

## First Candidate Subsystems

| Order | Subsystem | Why It Comes Early |
| --- | --- | --- |
| 1 | Filesystem | Already has backend-style boundaries and C++ nearby in `VFileSystem009.cpp`. |
| 2 | Public utility helpers | Narrower behavior surface and useful test coverage target. |
| 3 | Platform services | Good place to isolate OS, dynamic library, clock, and file handles. |
| 4 | Renderer state | High value, but needs screenshot/render smoke tests first. |
| 5 | Host/client/server state | Biggest payoff, highest compatibility risk, should follow smaller pilots. |

## Open Planning Questions

- What minimum C++ standard should this fork target across Windows, Linux,
  Android, and other supported platforms?
- Should exceptions and RTTI remain disabled for engine internals?
- Which automated smoke tests can prove that Steam assets plus FWGS-compatible
  game DLLs still reach menu and first frame?
- Should generated setup/runtime folders stay purely local, or should we add
  more setup scripts for Linux and CI parity?
