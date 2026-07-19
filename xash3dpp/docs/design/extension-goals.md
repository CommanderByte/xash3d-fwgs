# Extension Goals — North-Star Requirements

> **Date**: 2026-07-05\
> **Scope**: all xash3dpp subsystems — present and future\
> **Status**: the goals are directional and **unscheduled**; the door rules
> (§3) are binding on new code via Q-21 (EXTENSION_POSTURE)\
> **Related**: `decisions-architecture.md` Q-21 (binding hook) and Q-2 / Q-7 /
> Q-10 / Q-11 / Q-20; `threading-model.md` §5, §6.3, §8, §9;
> `debug-stats-design.md`

______________________________________________________________________

## 1. Why this document exists

The rewrite has a second purpose beyond GoldSrc parity: it is the foundation
for a set of experimental engine features that the legacy architecture makes
impractical. Parity-first chunks legitimately keep legacy-shaped cores while
the frozen ABI demands it (Q-20 defers entity-store modernization to
post-parity; the server tick is pinned to the main thread by the GoldSrc DLL
contract) — but a *deferral* must not become a *closed door*. Everyday rewrite
decisions can silently foreclose future work: one new file-scope global, one
context-less entry point, one debug feature that pokes internals instead of
extending a typed surface.

This document names the target features (§2), derives the shared
infrastructure they need (§3), and states the rules — the **doors** — that
everyday rewrite work must keep open. It deliberately does **not** schedule
any feature: scheduling happens in `implementation-plan.md` when a feature is
promoted to a chunk, and any feature with ABI or cross-subsystem impact gets
its own design brief first (`pm-determinism-decision.md` is the precedent).

______________________________________________________________________

## 2. The goals

| ID | Goal | Needs primitives |
|-----|------|------------------|
| G-1 | **In-engine MCP service** — external agents inspect and drive a running engine | P-1, P-2, P-4, P-6 |
| G-2 | **Game ABI v2** — a multithreading-suitable engine↔game interface, with a reworked HL SDK to match | P-3, P-5 (+ the Q-20 seam) |
| G-3 | **Dedicated debug thread** — live inspection without stalling the sim | P-1, P-2, P-4 |
| G-4 | **Expanded in-game debugging** — richer introspection, overlays, tooling | P-4 (+ Chunk 12/13 frontends) |
| G-5 | **Scripting runtime** — tooling-first embedded scripting; gameplay/modding later | P-1, P-3, P-4, P-6, P-7 |

### G-1 — In-engine MCP service

An MCP server embedded in (or attached to) the engine process so external
agents can query state (entities, cvars, world, stats) and execute actions
(console commands, spawns, level changes) against a live game.

- **What it needs**: an off-main listener (own thread; transport TCP /
  WebSocket / stdio); reads served from published snapshots (P-2) or
  internally-synchronised surfaces, never from live sim state; mutations
  marshalled to the main thread through the service inbox (P-1) — on Main
  they may execute via the existing command buffer, gated by the
  `ITrustOracle` seam cmd_cvar already ships for untrusted command sources;
  structured introspection surfaces (P-4); JSON serialization utilities
  (cold-path, so heap use is acceptable per Q-13).
- **What already exists**: `ITrustOracle` + `ICvarObserver`
  (`cmd_cvar/observers.hpp`); the three-tier stats model; `EntityView` as the
  typed entity read surface; Q-11 gives the placement answer (own protocol,
  own state machine, own deps ⇒ separate satellite target, e.g.
  `xash3dpp_mcp`).
- **Non-goals now**: no transport choice, no tool-surface design, no
  implementation. Dedicated-server-first is the natural v0 (it is what
  exists).

### G-2 — Game ABI v2 + HL SDK rework

A modernized engine↔game interface designed for multithreading, shipped as a
**load-time flavor** alongside the frozen GoldSrc ABI (one game DLL per
process — Q-20's framing), with the HL SDK reworked against it.

- **Why the GoldSrc ABI cannot be threaded**: context-less slots (state via
  globals — `gpGlobals`, the engine-side bridge); non-reentrant callbacks
  (any think may touch any entity, no declared access sets); one global
  `pmove_t`; blocking synchronous callback model. A v2 interface would carry
  a context handle in every slot, declare or transact entity access, use
  per-player movement contexts, and split the tick into schedulable
  compute/commit phases.
- **What keeps the door open on the engine side** (all already decided —
  the work is *keeping* them): the ABI-exact edict array behind the
  `EntityView` seam with raw access confined (Q-20 — "handleization or a new
  ABI format is a post-parity, load-time binding/arena flavor behind this
  seam"); context-first engine functions (threading-model §8.1: "engine
  functions called from thinks should accept explicit context parameters");
  compute/commit separation in the tick (threading-model §9 Rule 3);
  the versioned plugin descriptor (Q-10) as the v2 bootstrap shape;
  per-body physics contexts at Chunk 11 (threading-model §8.2).
- **Non-goals now**: no v2 API design in this doc. Promotion requires its own
  design brief; constraints from the existing solo SDK-rework prototype
  should be captured as an input to that brief (§5).

### G-3 — Dedicated debug thread

A thread that continuously inspects engine state (entity dumps, perf
counters, memory reports) and serves debug frontends without stalling the
sim.

- **What it needs**: a new `ThreadRole` value (the enum is additive); reads
  via P-2 snapshots and the already-atomic stats counters; actions via the
  P-1 inbox; cvar reads from off-main trigger the `shared_mutex` retrofit the
  threading model already schedules for "when a non-main caller appears"
  (§8.3) — G-1/G-3 are that caller.
- **What already exists**: `ThreadRole` registration + `assert_thread_role`
  enforcement; tiered stats with atomic counters; the filesystem is already
  safe for off-main readers.
- **Non-goals now**: none of it is built before a consumer exists; G-3
  becomes cheap once P-1/P-2 land for any other reason.

### G-4 — Expanded in-game debugging

Console tooling, overlays (Chunk 13), and UI panels (Chunk 12+) over the same
data the MCP service and debug thread read.

- **The unifying rule**: introspection is **one layer, many frontends**.
  A console command, an overlay, an MCP tool, and a debug-thread export all
  consume the same typed surfaces (P-4). No frontend grows a private backdoor
  into subsystem internals.

### G-5 — Scripting runtime (tooling first)

An embedded scripting runtime, welcomed into the engine in two stages:
**(a) tooling first** — debug automation, scripted test scenarios, console
scripting on the cold path; **(b) gameplay/modding later**, alongside the
modernized game ABI (G-2), with its own promotion brief.

- **Hard constraints** (encoded now so the runtime pick cannot box us in):
  engine targets stay `/EHs-c- /GR-` — the VM lives in an **isolated-island
  satellite** static lib with its own flags, behind `noexcept` binding shims;
  no exception (or longjmp across engine RAII) ever unwinds an engine frame.
  The runtime must expose a **complete allocator hook** bridgeable to the
  memory pools (accounting per pool at minimum; note pools guarantee only
  ≥8-byte payload alignment — no aligned-alloc API). Cold-path-only until
  proven; deterministic-hardenable for the warm path (fixed hash seeds,
  explicit RNG seeding, no reliance on unspecified iteration order); MSVC
  x64 + x86; permissive license; active upstream.
- **What already exists**: cmd_cvar reserved the affordances —
  `CvarWriteSource::Script`, `CvarType`, the `CvarDesc`/`CommandDesc`
  snapshots, the `ParamSpec` slot — plus `ITrustOracle` (untrusted-source
  gating), `ICvarObserver`, `core::log_set_callback` (REPL capture),
  `Filesystem::load_file`/`search`, and platform dynlib. Script surface v0 =
  `cmd_add`/`cbuf_*`/`cvar_*` + those seams, all P-4-conformant.
- **Runtime shortlist** (researched + adversarially verified, 2026-07-06;
  final pick via a prototype spike recorded in
  `scripting-runtime-brief.md`): **Lua 5.4 built as C** (front-runner —
  complete `lua_Alloc` with per-VM userdata and type-coded stats, no JIT,
  VM-per-thread, longjmp discipline required: raw C API binding, no RAII
  across error paths), **QuickJS-ng** (verified-complete per-VM allocator,
  return-code error model, spec-deterministic iteration order),
  **AngelScript** (cleanest error model + best C++ binding ergonomics;
  allocator is global/size-only — its spike derisks per-VM accounting).
  Quirrel was eliminated by verification (its mandatory compiler lib
  requires C++ exceptions); add **Luau** as a fourth candidate iff
  untrusted-mod sandboxing becomes a warm-path requirement.
- **Non-goals now**: no runtime pick, no bindings, no implementation, no
  MCP transport coupling (G-1 stays separate). The satellite target name
  `xash3dpp_script` is reserved, decided-not-built (`xash3dpp_http`
  precedent, Q-11).

______________________________________________________________________

## 3. Shared primitives and door rules

Each primitive is the infrastructure at least two goals share. The **door
rule** is what binds today; the primitive itself is built when its first
consumer is scheduled.

### P-1 — Main-thread service inbox (MPSC)

A typed multi-producer/single-consumer queue drained by the main loop at a
defined frame point — the marshal-to-Main primitive the threading analyses
repeatedly cite ("must marshal onto the Main thread"). The typed sibling of
the legacy `Cbuf` text path; threading-model §5.1 already specs MPSC queues
for audio commands and NetIO.

> **Door rule**: subsystems must not invent private cross-thread mutation
> channels. Until the inbox exists, off-main mutation of engine state is
> simply forbidden (which is today's OQ-9 posture). When the worker pool
> lands (Chunk 7), the inbox is designed alongside `JobToken` as part of the
> same queue family.

### P-2 — Published-snapshot reads

Off-main readers get immutable snapshots published at frame boundaries — the
double-buffered `RenderFrame` shape (threading-model §6.3); the server's
per-client frame ring is the second precedent. Readers never take locks into
live sim state.

> **Door rule**: any feature exposing sim state off-main must use a
> snapshot/immutable-publish design or an explicitly internally-synchronised
> surface. Direct references into live mutable state never cross a thread
> boundary. (Corollary of threading-model §9 Rules 1–3.)

### P-3 — Context-first entry points

Every new engine function reachable from DLL callbacks or service frontends
takes explicit context (parameter or owning object) rather than file-scope
state. This is threading-model §9 Rule 4 plus the §8.1 note, elevated because
G-2 depends on it: a context-carrying ABI can only be retrofitted onto
context-carrying internals.

> **Door rule**: no new file-scope mutable state beyond the documented ABI
> exceptions (the server's `g_bridge` + static return buffers pattern —
> forced by capture-less C slots). Every exception is listed in the
> subsystem's architecture-doc "Module statics" table with its justification.
> A v2 ABI slot design must carry context so the exception class shrinks
> rather than grows.

### P-4 — Typed introspection surfaces

Entity state via `EntityView`, cvars via the registry + `ICvarObserver`,
perf/memory via the three-tier stats model, logs via the logging sinks.
Debug and service features **extend** these surfaces when something is
missing; they do not reach around them.

> **Door rule**: a debug/service feature that needs state not currently
> exposed adds a typed query to the owning subsystem's surface (or a
> snapshot field), never an `extern` poke or a friend backdoor. Raw
> `entvars_t` access stays confined per Q-20 regardless of how convenient a
> debug dump would be.

Stats note: **collection** stays compile-gated per the three-tier model (no
runtime booleans on increments — deliberate); what is runtime is the
query/report side (`stats()` snapshots, query commands, the future
MCP/debug-thread consumers). Runtime-switchable deep-stats collection is an
explicit revisit trigger, never drift.

### P-5 — Narrowest-state signatures

Free functions over a runtime aggregate take the smallest sub-aggregate they
touch (`ClientMachinery &`, `SnapshotState &`), not the whole runtime;
whole-aggregate signatures are reserved for genuine orchestrators (lifecycle,
frame loop). This makes read/write sets visible in the type system — the
prerequisite for both snapshotting state (P-2) and scheduling tick phases in
parallel (G-2), and the main structural antidote to god-aggregate drift.

> **Door rule**: applies to all new free functions over aggregates. Existing
> signatures migrate in **Chunk 6B** (the scheduled full-retrofit wave, §4).
> Elevated to a binding standard by Q-22.

### P-6 — Services are satellites

MCP, debug services, and future experimental features live in separate
targets that consume public subsystem seams (per the Q-11 test — own
protocol, own state machine, own external deps ⇒ separate target). The
engine never links *toward* a service.

> **Door rule**: experimental features must pass the Q-11 test at
> boundary-spec time; an experiment that only works with tentacles into a
> subsystem's privates is redesigned before it lands.

### P-7 — Pool-owned classes with RAII lifecycle

State with invariants lives in a class with an RAII lifecycle;
free-functions-over-aggregate style is reserved for orchestrators. Pool-owned
classes follow the canonical idiom the tree already ships (`File`,
`ISearchBackend`; `memory.hpp` `PoolDeleter` note): a `create_<thing>`
factory holding the injected `PoolHandle` constructs via `pool_new<T>`; the
class overrides **both** `operator delete` overloads routing to `mem_free`;
a plain `std::unique_ptr<T>` then owns it.

> **Door rule**: class-scoped `operator new` is forbidden (it cannot carry
> the injected handle — it would force a global/TLS pool, violating Q-2).
> `std::make_unique<T>` stays banned except for pimpl `Impl`.
> `pool_new<T>` requires `alignof(T) ≤ 8` (the pools guarantee only ≥8-byte
> payload alignment). Binding rules and the full smart-pointer policy: Q-22.

### P-8 — Annotation discipline

The QN annotation matrix (`@lifetime:` / `@thread-safety:` /
`@pre-reserved:` / `// Pre:` / `// SAFETY:`; `// Post:` retired) plus
thread-role assertion coverage, applied uniformly — "documents-but-never-
asserts is non-compliant."

> **Door rule**: applies to all new code from QN's adoption; the completed
> subsystems are backfilled by Chunk 6B. Coverage is measured with
> denominators (annotated + `@annotation-exempt:`-marked / total required),
> never raw counts.

______________________________________________________________________

## 4. Per-chunk hooks

What each already-planned chunk owes this document — one line each, checked
at that chunk's boundary-spec / plan-implementation step:

| Chunk | Hook |
|-------|------|
| 6 (post-milestone) | The deferred S9-completion backlog stays chunk-inherited (unchanged); the structural half of the old pairing moved to Chunk 6B |
| **6B — hardening retrofit** | Every completed subsystem brought to Q-22/QN/QO conformance (lifecycle promotion, pool routing, annotation backfill, thread asserts, P-5 narrowest-state sweep); parity-gated subsystems behaviour-preserving with gates re-run; scope-fenced against the stub backlog |
| 7 — content | Worker pool + `JobToken` land ⇒ design the P-1 inbox as part of the same queue family; content loaders stay context-first (P-3). *Status 2026-07-19: the pool did NOT land with Chunk 7 — the queue-family design moved to the HB-4 brief / Q-24 (Chunk 9 lands the family in core; JobToken accedes to it when the pool is scheduled)* |
| 8 — save | The field-map serializer is state→bytes machinery; **consider** shaping it for reuse by debug dumps / snapshots (P-2/P-4) — do not contort it if parity says otherwise |
| 9 — sound | First production MPSC queue (audio commands) — validates the P-1 queue *primitive* (genericity pinned by a non-audio instantiation test in core); the Main-inbox drain-slot contract is explicitly NOT discharged (stays with the HB-4 brief until a G-1/G-3 consumer) |
| 10 — input | Bindings/key/key_dest state as typed snapshots (P-4: `bindings_snapshot()` + key-state query); input commands registered via cmd_add = G-5 script entry points; `IEventSource` synthetic-event injection is the scripted-scenario / service actuation door (G-5/G-1, Main-marshalled per P-1) |
| 11 — physics | Per-body `PhysicsContext`, no new global pmove state (threading-model §8.2) — **binding**, this is G-2's physics door |
| 12 — client | The thread-model decision lands before this chunk (existing landmine); the listen-server path is the first real off-main pressure — expect the §8.3 cvar `shared_mutex` retrofit and the first P-1 consumers |
| 13 — renderer | `RenderFrame` is the P-2 reference implementation; the overlay is G-4's first visual frontend |

`ThreadRole` additions (Service/Debug) are additive and may land with their
first consumer at any point.

______________________________________________________________________

## 5. Inputs wanted

- **HL SDK rework prototype** — the existing solo prototype's constraints
  (what it changed in the SDK, what engine surface it assumed, where it
  fought the GoldSrc ABI) should be written up as input to the future ABI-v2
  design brief. Until then, G-2's engine-side doors (P-3, P-5, Q-20
  confinement) are deliberately prototype-agnostic.
- **MCP service scope** — dedicated-only vs listen-server, and whether the
  v0 rides an external sidecar process (engine exposes a local socket; the
  MCP server is a separate binary) before moving in-process. Decide at
  promotion time.

______________________________________________________________________

## 6. What this document is not

- Not a schedule — no goal here has a chunk number until
  `implementation-plan.md` gives it one.
- Not a license to gold-plate — a door rule is a *constraint on shape*, not
  an instruction to build speculative infrastructure (the P-1 inbox is not
  built until a consumer schedules it).
- Not a parity waiver — where a door rule and byte-exact GoldSrc parity
  conflict inside a parity-gated subsystem, parity wins and the deviation is
  recorded here as a known door-debt (none currently).
