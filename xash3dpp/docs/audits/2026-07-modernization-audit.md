# Modernization & Extensibility Audit — 2026-07-20

> **Status**: LIVE (opened 2026-07-20, HEAD `cc73c054`)\
> **Precedent**: `2026-07-consolidation-audit.md` (verdict matrix, six-column
> ledger, four-way disposition) and `2026-07-chunk8-10-campaign.md` (phase
> log, numbered ledger, close-out sections)\
> **Scope**: all 16 implemented subsystems + the 6 zero-TU skeletons\
> **Approved by the user 2026-07-20**: full scope (reports + binding-surface
> diffs + mechanical wins landed + the three open door-class briefs), both
> chunk-precondition decision packets, full-scale fan-out.

Tree-wide audit of modernization and extensibility posture ahead of Chunk 11,
along four themes: readiness for north-star goals **G-2..G-5** (G-1
opportunistic only); whether the tree is **shaped** for a future fully
multithreaded engine while legacy parity keeps it single-threaded; **modern
C++ modularity** — interfaces and registries where appropriate; and
**reorganization that breaks nothing**, including the compatibility-layer
strategy that lets internal shape change behind a frozen ABI.

**Why now.** Ten of the thirteen existing `<sub>-modernization.md` reports
are dated 2026-07-07 — stale by Chunks 7, 8, 9, 10 and a consolidation
audit — and save, sound, input and imagelib have no report at all. That
staleness is the evidence for this campaign's governing rule: *advisory
output gets ignored; only findings landed on gated surfaces survive.* Hence
`promotes_to` is a required field with no `report-only` default.

______________________________________________________________________

## Design rules (what makes this audit different)

1. **A per-subsystem agent may only be asked questions answerable from
   inside that subsystem.** "Is the tree ready for G-3?" and "are we
   multithread-shaped?" are *sweep* questions; asking them sixteen times
   buys sixteen partial, mutually inconsistent answers plus a dedupe bill.
   North-star readiness is therefore a **tag** (`ext_tags`) plus Phase-3
   lenses, and multithread readiness is **one finding field**
   (`thread_relevance`) plus one lens.
2. **Inventory is not a finding.** Every pack returns a separate fixed-schema
   facts block. Cross-subsystem patterns fall out of a set comparison over 16
   blocks, not out of an agent happening to remember one array while reading
   another file.
3. **The parity fence is kernel-scoped, not subsystem-scoped**
   (`decisions-architecture.md:946-954`). A subsystem-wide fence would block
   every `server` finding — including the narrowest-state work that is the
   point.
4. **Anything proposing to BUILD needs a named day-one consumer** or it is
   recordable only as a shape constraint, never as work (`extension-goals.md`
   §6). The tree has already failed this test once: 13 `I*` interfaces have
   zero production implementations and the doc's own named "P-2 reference
   implementation" (`RenderFrame`) is zero code.
5. **Subtraction is a first-class lens.** Every other lens is biased toward
   adding structure. L11's success metric is negative.

______________________________________________________________________

## Phase log

| Phase | Status | Output | Notes |
|---|---|---|---|
| 0 — fact base | ✅ | `facts.json`, per-subsystem scans, common brief | see census below |
| 1 — packs | ⏳ | 36 read-only packs (A1 currency / A2 shape / A3 seam) | tiered A/B/C |
| 2 — verify | ⏳ | adversarial refute, gold-plating critic, parity+ABI fence, Q-21 posture | |
| 3 — lenses | ⏳ | L1..L11 cross-cutting | |
| 4 — decisions | ⏳ | 2 decision packets + HB-5/6/7 briefs | main agent |
| 5 — assembly | ⏳ | 16 reports, mechanical wins, binding diffs, exit gates | |

______________________________________________________________________

## Phase 0 — fact base (2026-07-20)

Deterministic scans (`census`, `dep_scan`, `stub_scan` ×16, `q21_scan`,
`compliance_scan` ×16, `limits_scan`) plus one mechanical grep pass, captured
once so 36 agents do not each re-derive them.

| Fact | Value | Confidence |
|---|---|---|
| src TUs / tests per arch | 167 / 124 | exact |
| lib targets | 17, acyclic, L0 utilities/memory/miniz → L7 abi/launcher | exact |
| `I*` interfaces | 46, of which **13 have zero production impls** | zero-impl heuristic |
| `assert_thread_role` sites | **256** against **107** `compliance-allow(thread-assert)` waivers | exact |
| production thread spawns | **2**, both `src/sound/topology.cpp` | exact |
| stats structs | **15** — 6 atomic, **9 plain** (plain blocks G-3 reads) | exact |
| `switch` sites | 61 | exact |
| file-scope mutable statics | ~26, ~21 without a nearby `compliance-allow` | heuristic |
| `chunk11..14` markers in code | 69 | exact |
| `concept` / `ranges` / `std::function` / CRTP | **0 / 0 / 0 / 0** | exact |
| `std::expected` | 6 of 15 subsystems | exact |
| `ServerRuntime &` | 54 header / 132 src | upper bound |

Every heuristic number carries a `_confidence` entry in `facts.json` and is
re-verified by the owning subsystem's pack; corrections are recorded in the
ledger rather than silently applied.

**Structural facts carried into the lenses**: `sound` and `input` are leaf
libs linked by nothing until Chunk 12; `imagelib` has no in-tree consumer at
all (a Chunk-13 obligation, *not* dead code); the six zero-TU skeletons have
no boundary spec, so `q21_scan`'s 16/16 is silence rather than health.

______________________________________________________________________

## Verdict matrix

*(populated at Phase 2 close)*

______________________________________________________________________

## Cross-cutting themes

*(populated at Phase 3 close)*

______________________________________________________________________

## Adjudication ledger

Disposition is four-way and mandatory — **fix-doc · fix-code ·
compliance-allow · false-positive**, plus `escalate` and
`deferred`. **No silent drops.**

| # | Phase | Ref | Sev | Verdict | Disposition | Adjudication / claim |
|---|---|---|---|---|---|---|

______________________________________________________________________

## Deferred with owner

*(populated at close)*
