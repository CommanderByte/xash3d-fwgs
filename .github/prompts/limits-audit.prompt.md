---
name: "Limits audit — magic number enforcement"
description: "Scan xash3dpp/ (or a specific subsystem) for magic number literals that should be in limits.hpp, verify existing XASH_LIMIT_* entries are used correctly, and report any missing or misplaced limits. Read-only."
argument-hint: "optional subsystem name to scope the scan, e.g. 'filesystem'. Omit to scan all of xash3dpp/."
agent: agent
tools: [read, search, execute]
model: claude-haiku-4-5-20251001
---

# Limits Audit: `$ARGUMENTS`

Verify that all fixed buffer sizes, pool capacities, and count limits are
declared in `xash3dpp/include/xash3dpp/limits.hpp` with the `XASH_LIMIT_*`
override pattern. **Read-only — no files are modified.**

Scope: `xash3dpp/src/$ARGUMENTS/` and `xash3dpp/include/xash3dpp/$ARGUMENTS/`
(or all of `xash3dpp/` if no argument given).

---

## Step 1 — Run the scanner

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\limits_scan.py $ARGUMENTS --json
```
*(MCP: xash-tools tool `limits_scan` — same data.)*

(`python` = the repo venv, `.venv\Scripts\python.exe`; omit the subsystem
argument to scan all of `xash3dpp/`.) The tool parses every
`#ifndef XASH_LIMIT_* / inline constexpr / #else / #endif` block out of
`limits.hpp` (name, default, group, usage count) and reports:

- `magic` — integer-literal candidates in scope (array sizes, template
  capacities, constexpr values, reserve/resize calls)
- `shadow` — literals equal to an existing limit's default
- `unused` — limits with zero `limits::<name>` references (dead-limit
  candidates)

## Step 2 — Classify the candidates (QO CONSTANT_PLACEMENT)

Classify each `magic`/`shadow` hit per the QO decision table
(`decisions-style.md §CONSTANT_PLACEMENT`) — `limits.hpp` is one of four
homes, not the default answer:

- **N/A** — not a constant worth naming: mathematical constants, powers of 2
  in bit ops, loop counts over fixed structure (RGB channels),
  offsets/sentinels; or literals only in test files (already excluded).
- **frozen** — ABI/wire-frozen values (opcodes, protocol discriminators,
  codec parameters, struct dims): vendored `k_*` / `static constexpr` beside
  the ABI or protocol header that uses them (LZSS window, packet magic) —
  NOT `limits.hpp`, never tunable.
- **belongs-in-limits** — structural capacity whose change requires
  re-init/realloc (array dims, pool caps, ring sizes) → `limits.hpp` under
  the subsystem group with the `XASH_LIMIT_<NAME>` override pattern
  (compile-time override only).
- **belongs-as-cvar** — a behavioral tunable (rate, timeout, speed, toggle)
  masquerading as a constant → recommend a cvar (legacy-family prefix +
  snake_case; `dev_*` for developer knobs), not a limit.
- **ceiling+dial** — an operator-sized capacity → BOTH: ceiling in
  `limits.hpp` + a cvar dial clamped ≤ ceiling (the legacy
  `MAX_CLIENTS`/`sv_maxclients` pattern).

For each `unused` limit, verify by search that it is genuinely dead (it may
be referenced from CMake or reserved by a boundary spec for the next chunk —
say which) → **WARNING — dead limit** only when truly unreferenced and
unreserved.

---

## Step 3 — Report

```text
Limits Audit — $ARGUMENTS
==========================

limits.hpp entries: <N>

Missing limits (magic numbers not in limits.hpp):
| File | Line | Literal | QO class | Suggested name | Suggested home |
|------|------|---------|----------|----------------|----------------|
| ...  |      |         | limits / cvar / ceiling+dial / frozen | | |
(or "None found ✓")

Warnings on existing limits:
| Issue | Limit name | Details |
|-------|-----------|---------|
| ...   | ...        | ...     |
(or "None ✓")

Recommended additions to limits.hpp:
<For each missing limit: the #ifndef / inline constexpr / #else / #endif block
 to add, under the appropriate subsystem group comment.>
```
