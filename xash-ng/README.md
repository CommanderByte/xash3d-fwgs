# xash-ng — Modular C++ Rewrite

This tree is the starting point for the modular C++ rewrite of Xash3D FWGS.

The existing engine code remains at the repository root and is the authoritative
legacy reference. New modules land here, subsystem by subsystem, while the
legacy tree stays buildable and runnable throughout.

## Ground Rules

- Preserve C-compatible ABI surfaces (game DLL, client DLL, renderer, filesystem
  plugin, shared SDK headers). Internal implementation language can be C++.
- Keep exceptions and RTTI disabled for engine internals unless a later decision
  explicitly revisits this.
- Every subsystem migration must be independently buildable and manually
  smoke-testable before the next subsystem starts.
- Record compatibility-sensitive quirks instead of silently removing them.

See [Documentation/codex/modular-cpp-rewrite-feasibility.md](../Documentation/codex/modular-cpp-rewrite-feasibility.md)
for full rationale, and [Documentation/codex/modularization-plan/](../Documentation/codex/modularization-plan/)
for the phased migration roadmap.

## Subsystem Migration Order

| Phase | Subsystem     | Directory         | Status      |
|-------|---------------|-------------------|-------------|
| 1     | Filesystem    | `filesystem/`     | Not started |
| 2     | Public utils  | `public/`         | Not started |
| 3     | Platform      | `platform/`       | Not started |
| 4     | Renderer      | `ref/`            | Not started |
| 5     | Engine core   | `engine/`         | Not started |

## Directory Layout

```
xash-ng/
  filesystem/   Filesystem backend rewrite (pilot subsystem)
  public/       Rewritten public utility library (string, CRC, math, UTF-8, …)
  platform/     OS, dynamic library, clock, and file-handle abstractions
  ref/          Renderer internals (after smoke-test coverage exists)
  engine/       Host, client, server state (highest risk, comes last)
```
