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
| 0 — fact base | ✅ | `facts.json`, per-subsystem scans, common brief, read-only pack prompt | commit `6d9c383a` |
| 1 — packs | ✅ | 36 read-only packs → **173 findings** + 35 facts blocks | 1 pack failed, re-run standalone |
| 2 — verify | ✅ | 13 agents: adversarial refute ×6, gold-plating critic ×2, parity+ABI fence ×3, Q-21 posture ×2 | 6 REFUTED / 29 AMENDED of 85 verified |
| 3 — lenses | ✅ | L1..L11 → 53 recommendations, 55 shape constraints, 24 open decisions, **58 obligations** | |
| 4 — decisions | ✅ | 2 decision packets + 3 door briefs (HB-5/6/7) | main agent |
| 5 — assembly | ✅ | 17 reports, 2 defect fixes, binding diffs, exit gates | commits below |

**Agent spend**: 49 + 11 + 17 = **77 agents**, ~10.5M subagent tokens,
~62 min wall clock across three Workflow runs.

______________________________________________________________________

## Commits

| Commit | What |
|---|---|
| `6d9c383a` | Phase 0 — fact base, read-only pack prompt (+ adapters, SYNC-CORE 22→23), audit doc |
| `6dfed16f` | **defect** — `EngineBridge::sv_time` never stamped; legacy edict reuse grace inoperative |
| `1b417663` | **defect** — archive extensions compared case-sensitively vs legacy `Q_stricmp` |
| `c0117e4c` | 17 subsystem reports, 2 decision packets, 3 door briefs |
| *(this)* | binding-surface diffs — chunk entry gates, HB-5/6/7 status, extension-goals §4 |

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

`Nc/Mf` where given = claims verified / findings. Tier counts are
post-adjudication.

| Subsystem | Findings (H/M/L) | Report | Notable |
|---|---|---|---|
| server | 4/6/8 new, 3 carried | refreshed | 3 false claims in its own Extension-axes table; `sv_time` defect |
| networking | 4/5/6 | refreshed | A3 pack re-run; the scanner-artifact waiver count; 1 MiB dead pool |
| cmd_cvar | 7/16/7 | refreshed | `get_compat_policy()` unreachable; `ICvarObserver` dead in every direction |
| content | 7/13/12 | refreshed | studiohdr offset table in three unlinked copies |
| filesystem | 5/9/10 | refreshed | **live parity divergence** (fixed) |
| core | 2/3/5 | refreshed | `Clock::stats()` returns a live ref into non-atomic memory |
| host | 2/5/5 | refreshed | shipped launcher never builds `EngineContext` |
| map_loader | 2/–/7 | refreshed | |
| platform | 2/5/5 | refreshed | |
| memory | –/3/3 | refreshed | 2 findings retired; 3 dead pools |
| utilities | 1/6/5 | refreshed | |
| abi | –/1/1 | refreshed | no new findings — correctly frozen |
| launcher | 1/–/3 | refreshed | |
| **save** | 1/1/3 | **created** | |
| **sound** | 3/5/6 | **created** | |
| **input** | –/3/5 | **created** | its one High candidate was refuted |
| **imagelib** | 1/–/2 (+7 cross-ref) | **created** | zero in-tree consumers, all paths |

**Adversarial outcome**: 85 findings entered verification, **6 REFUTED** and
**29 AMENDED** — **41% needed correction before use**. Refuted findings are
recorded in each report's "Investigated and refuted" section with the reason,
so the next auditor does not re-derive them.

______________________________________________________________________

## Cross-cutting themes

### 1. Three features are silently disabled in the shipped binary

All confirmed after adversarial refutation attempts, none of which succeeded.

- **`get_compat_policy()` is unreachable** — no header declaration, no caller,
  so `GoldSrcCompatPolicy` is never instantiated and **every GoldSrc compat
  quirk is inert in production**. The Q-12 link-time selection mechanism does
  not select anything. It survived four separate refutation attempts.
- **`ITrustOracle` has zero production implementations** — a seam
  `cmd_cvar-boundary.md` calls the "headline G-1 provider, already exists".
  **Correction (2026-07-20 follow-up): this entry said the trust gate is
  permanently *open*. It is permanently *shut*.** With
  `trust_oracle == nullptr`, `cmd_dispatch.cpp:197` evaluates
  `stuffcmd_trusted` to `false`, so every `FCMD_PRIVILEGED` command is
  REJECTED and cvar writes are attributed to a non-Console source. That makes
  this a behaviour divergence, not a security hole: legacy grants privilege on
  a local single-player listen server (`SV_Active() && maxclients == 1`) and
  xash3dpp never does. Severity is lower than originally written, and the
  fix direction is the opposite of what "permanently open" implies.
- **The shipped launcher never constructs an `EngineContext`.** `Host::Main`
  builds init params with "dep pointers intentionally left null"; the
  fully-wired path is exercised by one test file, not the binary. The
  refuter's note: *"the code is worse than the finding claims."*

### 2. Two live defects, both fixed with discriminating regression tests

- **`EngineBridge::sv_time`** — read at four pfn slots, written nowhere,
  frozen at 0.0. Every game-DLL `free_edict` stamped `freetime = 0.0`, making
  the reuse guard's first clause unconditionally true, so **legacy's 0.5 s
  edict slot-reuse grace never operated at all** on the game-DLL path. It
  survived Chunk 6B, the consolidation audit, the chunk 8-10 close-out audit
  and 35 analysis packs — because a mirror that is *read but never written*
  carries no stub marker and matches no `compliance_scan` rule.
- **Archive extension matching** — legacy uses `Q_stricmp`; xash3dpp used
  `!=`, so `FOO.PAK` mounted under legacy and was silently skipped here, with
  a misleading "unsupported archive extension" warning. The sibling WAD table
  in the same subsystem already used `ci_equal`.

Still open and recorded: `snapshot_alloc_ring` sets `num_client_entities`
before its two allocations and returns failure without freeing.

### 3. A tooling defect invalidated every thread-assert coverage claim

`compliance_scan`'s mutator regex anchors the verb directly to `(`, and 16 of
its **26** verbs carry no suffix wildcard (this entry said 30; the alternation
holds 26). Definition sites are invisible wholesale — including `clear_world`,
`send_packet`, `transmit_client`, `connect_client`, and `register_thread_role`
itself. Every "N sites / M waivers" figure in every boundary doc is therefore a
scanner artifact, not a semantic inventory. Fixing the regex is one line;
adjudicating what it surfaces is not, so they must land separately.

**Discharged 2026-07-20.** The fix uses `(?:_\w+)?`, not the `\w*` proposed
here: measured over `src/**/*.cpp`, `\w*` adds 8 names beyond `(?:_\w+)?` and
all 8 are false positives (`sendto`, `Server::initialized`,
`Sound::initialized`, `Clock::starttime`, `addr_string`, `sends_qport`).
`reset` is the one exception, promoted to `reset\w*` because both of its
wildcard-form matches are genuine mutators. Surfaced sites went 199 → 269 and
findings 8 → 50 (not ~55); a second pass then filtered 7 structural
non-definitions (ternary continuations, a pure-virtual declaration, variable
declarations whose TYPE ends in a verb, a const getter). All 50 were
adjudicated: 5 asserts added, 45 `compliance-allow` with per-site reasons.

### 4. The zero-impl interface count was right; its meaning was not

Only **one** (`ICvarObserver` — zero impls, zero call sites, no chunk tag) is
genuine over-abstraction. **Eight are legitimate scheduled doors** that are
merely unrecorded as such, three are exercised nullable seams, and one
(`IMapLoaderObserver`) is a door nobody owns. The remedy is mostly *recording*,
not deleting — which is the opposite of what the raw count suggested.

### 5. Registries: three shapes, one of them a parity bug

Four extension-keyed dispatch sites, three incompatible shapes, and three
different answers to the same legacy `Q_stricmp` contract — one of which was
the live divergence in theme 2. Making the key column mandatory collapses all
three, deletes two `handles()` virtual hierarchies and one drift-prone
parallel array, and is a **net deletion**.

### 6. Subtraction

L11 identified ~690 lines, ~55 named symbols, 2 compiled TUs, 6 unreferenced
directories, 4 structs and 1 interface deletable with **zero behavioural
change** — plus a permanently-resident **1 MiB `PacketPool`** that nothing
ever acquires from, and `NetchanFlags::use_lzss` which is written by
`client_state.cpp` in the belief that it controls wire behaviour and is read
nowhere.

### 7. The audit's own fact base was substantially wrong, and the packs caught it

Every `naked_new` and `requires` hit was the English word in a comment;
interface implementation counts were inflated by counting pointer-member
declarations as implementations; the "107 thread-assert waivers" double-counted
9 dual-tag sites; and the statics scan flagged a `for`-loop variable while
missing 8 KB of genuinely undocumented file-scope state. Recorded in
`CORRECTIONS.md`, which overrode the fact base for every downstream agent.
**This is why the facts-block/finding split exists** — heuristics are a
starting point for verification, never a quotable result.

______________________________________________________________________

## Adjudication ledger

Disposition is four-way and mandatory — **fix-doc · fix-code ·
compliance-allow · false-positive**, plus `escalate` and
`deferred`. **No silent drops.**

**This table shipped empty.** It was declared mandatory and then left with a
header and zero rows, which made no finding traceable to a disposition — the
single largest self-inconsistency in this close-out. It is not reconstructed
row-by-row here, because the per-finding dispositions live in the artifact
that actually holds them: `2026-07-modernization-audit.ledger.json` (and its
human index `.ledger.md`), extracted 2026-07-20 from the Phase-3 lens corpus.
That corpus carries **100 obligations, 30 open decisions, 61 recommendations
and 63 shape constraints**, each with evidence and an owner.

| # | Phase | Ref | Sev | Verdict | Disposition | Adjudication / claim |
|---|---|---|---|---|---|---|
| 1 | follow-up | mutator-regex defect (theme 3) | High | CONFIRMED | fix-code | Fixed with `(?:_\w+)?`; 50 surfaced candidates all adjudicated (5 asserts, 45 allows). |
| 2 | follow-up | `snapshot_alloc_ring` OOM (deferred D-1) | High | CONFIRMED — worse than filed | fix-code | Count published before the buffers existed; `[[nodiscard]]` defeated by a `( void )` cast at the sole caller, so the failure path reached a null deref. Fixed with a discriminating regression test. |
| 3 | follow-up | Q-20 compliance rule "is the guard" | High | CONFIRMED | fix-code | The rule did not exist. Written as `entvars-confinement`; 0 violations, so the confinement had held by convention. |
| 4 | follow-up | `server-boundary.md` P-3 / P-5 / P-8 rows | Med | CONFIRMED | fix-doc | Three overclaims corrected against re-derived census numbers; P-1/P-4/G-3 re-checked TRUE and left alone. |
| 5 | follow-up | `IMapLoaderObserver` impl claim | Med | CONFIRMED | fix-doc | Nothing under `src/server/` implements it; recorded as an unowned door. |
| 6 | follow-up | `assert_main_thread` "thin wrapper" | Med | CONFIRMED | fix-doc | False in six docs plus a shipped header comment that also cited the wrong path. |
| 7 | follow-up | HB-2 fence anchor `clip.cpp:219` | Med | CONFIRMED | fix-doc | Real kernel is `clip.cpp:245-281`; Chunk 11 entry-gate item 2. |
| 8 | follow-up | `ITrustOracle` "gate permanently open" | Med | **REFUTED (inverted)** | fix-doc | The gate fails CLOSED. Behaviour divergence, not a security hole. |
| 9 | follow-up | "16 of 30 verbs" | Low | AMENDED | fix-doc | The alternation holds 26 verbs, not 30. |
| 10 | follow-up | "~58 obligations / 24 open decisions" | Low | AMENDED | fix-doc | The corpus holds 100 and 30; the lower figures counted only L10's register. |

______________________________________________________________________

## Forward-fit: the obligations register

The tree already owes the remaining chunks **100 concrete, file:line-anchored
obligations** (this section said ~58; that counted only L10's forward-fit
register and not the obligations the other ten lenses raised in their own
right), and before this audit **not one was read by any entry gate**. The ones
a chunk can act on live where a gate actually reads them — as `**Entry gate**`
blocks in `implementation-plan.md`'s Chunk 11/12/13/14 entries.

The full set is now also committed as
`2026-07-modernization-audit.ledger.json` + `.ledger.md`. Declining to create
that register was the original call — "document #14 by construction" — but it
had two costs paid immediately: eleven obligations were reported ownerless
when they simply had not been written down, and `platform-modernization.md`
cites an "audit obligations register" that did not exist. A generated,
machine-readable ledger is not document #14; it is the artifact the prose was
a lossy summary of.

Distribution: **Chunk 11 is the smallest debt** (11 items, 3 actionable today,
1 of which was a live correctness defect, now fixed). **Chunk 12 carries over
half the total, spread across nine *other* subsystems** — a
`/plan-implementation client` run scoped to `src/client/` would find almost
none of them. Chunk 13 carries the imagelib contract that has never been
exercised. Eleven obligations have **no owner at all**.

**Stale chunk tags are worse than untagged TODOs**: five in-code markers name
chunks that have already shipped, so they hide behind a discharged number and
are filtered out by any gate scoped to open chunks.

**The six 0-TU skeletons still have no boundary spec**, so `q21_scan`'s 16/16
is silence rather than health. Taking it to 22/22 is one commit — deferred
here with an owner rather than rushed, because it interacts with L11's
proposal to delete the same six directories, and `xtools.subsystems()` is the
only cheap denominator the scanner can adopt. **That conflict is recorded
rather than resolved**; whoever takes it must decide the denominator first.

______________________________________________________________________

## Re-run triggers (not dates)

A dated document is stale by definition; a triggered one is a gate. Each lens
carries its own trigger; the load-bearing ones:

| Trigger | Invalidates |
|---|---|
| A 3rd production `spawn_thread` outside `src/sound/topology.cpp` | the whole thread-model packet, and HB-5's "no primitive warranted" verdict |
| A decision-coupled thread-assert waiver appears outside `networking` | the "22 sites, one subsystem" scope |
| The networking cvar-read count moves off **zero** | flips the cmd_cvar retrofit from shape-constraint to work |
| A 5th extension-keyed dispatch site is added | the registry-unification analysis |
| imagelib gains its first production consumer | the renderer packet's entire "zero consumer" baseline |
| The stats-struct count changes, or `diagnostics_dump` lands | HB-6 |
| A boundary spec is created for any 0-TU skeleton | makes the render-thread option evaluable for the first time |

Eight of eleven triggers are mechanizable, but **six sit behind tool fixes**
— chiefly the mutator-regex defect above. The triggers are worth exactly as
much as the tooling wave that lands them.

______________________________________________________________________

## Deferred with owner

- **`snapshot_alloc_ring` OOM invariant break** — found and verified, not
  fixed here (it wants a `free_rings` audit alongside). Owner: server.
- **The `compliance_scan` mutator-regex fix** — one line, but it surfaces ~55
  new candidates needing adjudication. Owner: a tooling wave; land the fix and
  the adjudications as separate commits.
- **Six stub boundary specs → `q21_scan` 22/22** — blocked on the denominator
  decision above. Owner: whoever takes L10-R4.
- **The ~690-line deletion set** — enumerated and fenced, not executed. Owner:
  per-subsystem, tracked in each report's High tier.
- **`server`'s `snapshot_*` → `delta_frame_*` rename** (~165 sites) —
  deliberately a Chunk-12 precondition, not standalone churn beside a fenced
  codec.
- **HB-12 shared RNG** — its recorded owner is still the phrase "a future
  harmonization session", and its inventory is incomplete: a third stand-in in
  `sound/dsp.cpp` is invisible to it. Chunk 11 writes the fourth unless the
  target shape is recorded first.
- **Eleven ownerless obligations** — including Q-20's promised compliance-scan
  rule confining raw `entvars_t` access, which `server-boundary.md` **asserts
  guards the code** and which does not exist anywhere in the tooling.
