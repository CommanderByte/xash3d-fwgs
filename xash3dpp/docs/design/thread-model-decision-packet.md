# Decision packet — the Chunk-12 thread model

> **Date**: 2026-07-20 (tree-wide modernization audit, lens L8)\
> **Status**: DECISION OWED — this packet does not decide it; it makes deciding
> cheap. Recommended default and an undecided-fallback are both stated.\
> **Gates**: Chunk 12 (client). `implementation-plan.md` Chunk-12 complexity
> note: *"The thread-model decision (whether network I/O or rendering run
> off-main) must be made before this chunk starts."*\
> **Related**: `threading-model.md` §3.5/§3.6/§3.7, §7, §8.3, §11;
> `renderer-backend-decision-packet.md` (the other half — see §2)

______________________________________________________________________

## 1. The headline

The precondition as written **cannot be discharged**, and the half of it that
can be is **already answered by a binding design document the plan does not
cite**.

- `threading-model.md` **§3.5** states verbatim: *"Chunks 8/10/11/12 (save,
  input, physics, client) — Model B unchanged. No new threads. All subsystems
  run on `T_MAIN`."*
- `threading-model.md` **§3.6** already assigns the render thread to **Chunk
  13**: *"Chunk 13 (renderer) — Model C (optional render thread). `T_Render`
  may be added depending on the renderer plugin."*
- `threading-model.md` **§3.7**: *"`T_NetIO` is deferred until dedicated server
  load demonstrates a need."*

So the implementation plan requires, before Chunk 12, a decision that the
threading model already places in Chunk 13. **Two binding documents
contradict each other**, and the contradiction is what has kept the
precondition open.

______________________________________________________________________

## 2. Split the precondition — two decisions, different deadlines

| Half | Decidable now? | Why |
|---|---|---|
| **NetIO off-main** | **Yes** | Evidence is complete: §7.4's two named triggers are both unfired, networking's waivers are written to survive either answer, and networking reads **zero** cvars. |
| **Render off-main** | **No — by construction** | The renderer is a 0-TU skeleton with **no boundary spec**; `ThreadRole::Render` has no use outside the name table; §6's named reference implementation `RenderFrame` is **zero code**; and Chunk 13 carries its own undecided GL-vs-Vulkan precondition that this decision depends on. You cannot decide the thread affinity of a subsystem that has no shape. |

**Recommendation: split it.** The NetIO half closes at Chunk 12 by citing
§3.5/§3.7. The render half moves to Chunk 13's precondition list, beside the
backend decision it depends on — which is where §3.6 already puts it.

A precondition that cannot be satisfied is how a chunk stalls indefinitely.

______________________________________________________________________

## 3. Scope: the debt is 22 sites, not 98

The campaign's own headline figure treated the `compliance-allow(thread-assert)`
population as one undifferentiated backlog. Triaged, **99 real production
sites** split by cause, and only one bucket is coupled to this decision:

| Bucket | Sites | Meaning | Decision-coupled? |
|---|---|---|---|
| **A — thread-agnostic** | **62** | *"no thread affinity exists here"* — the opposite of a pin. networking codecs/wire/`MessageBuf` 33, platform OS-handle wrappers 12, input private leaves 9, filesystem immutable backends 2, utilities value types 2, misc 4 | No |
| **B — already synchronised** | 5 | `filesystem.cpp` under `shared_lock`/`unique_lock` on `paths_mutex` — asserting Main would *forbid* the concurrency the lock exists to permit | No |
| **C — design-pinned** | 10 | filesystem lifecycle ×2; sound DSP/registry/vox ×8, whose role is genuinely **conditional** (decoder while the topology runs, Main when it does not) | No |
| **D — DECISION-COUPLED** | **22** | **all in `networking`** | **Yes** |

Bucket D splits evenly and already names its own seam in the rationale text:

- **11 transport sites** (`context.cpp`, `netchan.cpp`, `loopback_transport.cpp`,
  `split_reassembler.cpp`) — *"role unasserted until the NetIO thread is split
  out"*.
- **11 delta sites** (`delta_codec.cpp`, `delta_tables.cpp`) — *"sim-thread …
  **NOT NetIO** … asserts land at the sim/NetIO flip"*.

**Closing this precondition touches 22 sites in one subsystem, not 98 across
ten.** The input pack's conclusion — that its 9 waivers are one well-reasoned
pattern rather than debt — generalises to all 62 of bucket A.

______________________________________________________________________

## 4. Consequence table

| | (a) Main-only through Ch.12 | (b) NetIO off-main | (c) Render off-main | (d) both |
|---|---|---|---|---|
| Waivers discharged | 0 of 22 | **22 of 22** | 0 of 22 | 22 of 22 |
| New asserts owed | 0 | ~22 in a subsystem with **zero** today | unknowable (0 TUs) | ~22 + unknown |
| Forces the cmd_cvar retrofit? | No | **No** — networking reads zero cvars | **Yes** — per-frame `gl_*` reads | Yes |
| Forces memory-pool sync? | No | Yes if transport pool-allocates | Yes | Yes |
| Plain stats structs forced atomic | 0 | 0 (networking already atomic) | ≥4 | ≥4 |
| Findings that become URGENT | none | 1 | the 13 `blocks-G3-read` set | all |
| Re-tiered up | none | HB-5 gains a 2nd real producer | `Clock::stats()` High → **blocker**; HB-5 must be **built** | all |
| HB-2 exposure | none | **wire bit-codec + LZSS + delta widths execute on two threads**; the byte-exact witness must be re-derived per topology | none | both |
| Test-matrix cost | 0 | ×2 on transport (loopback stays Main per §7.2) | renderer has no tests | ×2+ |
| Blocks Chunk 12 start? | No | No (Chunk-12-parallel) | **Yes** | Yes |

The decisive row is the cvar one. A tree-wide sweep finds **40 production cvar
read sites: sound 17, server 23, networking 0.** Moving NetIO off-main does not
force the cmd_cvar retrofit. Moving rendering off-main does.

______________________________________________________________________

## 5. Recommended default — (a), NetIO converted from *open* to *triggered*

Stay Main-only through Chunk 12. Cite §3.5 and §3.7 rather than re-deciding,
and let §7.4's two existing named triggers (an async DNS/HTTP consumer appears;
player count reaches 64+) govern when NetIO is revisited.

Five reasons:

1. **The binding doc already says it.** The plan's precondition is closed by
   citation, not by deliberation.
2. **No consumer forces (b).** Both §7.4 triggers are unfired, and networking's
   22 waivers are explicitly written to survive either answer.
3. **The tree already solved the only real cross-thread configuration problem
   without any of this.** Sound reads its 17 cvars on Main, packs them into a
   `MixConfigSnapshot` POD, and pushes it over the MPSC `AudioCommand` queue —
   applied decoder-side, **zero cmd_cvar changes**, and closed by a byte-exact
   witness. That is a shipped proof that snapshot-and-push beats locking the
   registry.
4. **(c) is undecidable at Chunk 12 by construction**, so bundling it
   guarantees the precondition never closes.
5. Per `extension-goals.md` §6, (b)/(c)/(d) are speculative infrastructure with
   no day-one consumer. (a) is the only option that is not.

**Choosing (a) costs nothing later.** §7.3's abstraction-boundary commitment is
already **met** — `recvfrom`/`sendto` appear outside `platform/` in exactly two
places, both behind `IPlatformSockets` — so §7.3's claim that adding T_NetIO
later "is a matter of replacing the implementation of these two functions"
holds. And §6.5's rule ("design the ownership boundary first; threading is
mechanical") keeps (c) reachable.

### Fallback if still undecided when Chunk 12 starts

**Proceed on: Model B unchanged, all client work on `T_Main`, both `T_NetIO`
and `T_Render` deferred.** This is not a coin-flip default — it is what §3.5
already binds, and it is the only assumption under which all 99 waivers, all 13
`blocks-G3-read` findings, and both zero-TU skeleton subsystems remain valid
without a line changing.

______________________________________________________________________

## 6. Gates that attach regardless of the answer

Four load-bearing doc claims are **false today** and would make a later
(b)/(c)/(d) unsafe. They must be corrected whichever option is chosen.

| Doc | Claim | Reality |
|---|---|---|
| `threading-model.md:566` | "memory — allocate \| Any (**pool spinlock**) \| **Today**" | **No synchronization primitive exists in `src/memory/`** — a grep for mutex/shared_mutex/atomic_flag/spinlock across the memory sources and headers returns **zero** matches. Not a live bug only because nothing pool-allocates off-main: sound's decoder allocates via the system heap. Every off-main option assumes this row is true. |
| `threading-model.md:11, 81-83`, `thread_role.hpp:21-23`, **and `decisions-architecture.md` Q-6** | `assert_main_thread()` "becomes a thin wrapper" over / is "replaced by" `assert_thread_role(Main)` | It is an **independent** implementation over a lazily-captured function-local `std::thread::id` that **silently no-ops until capture**. `thread_role.hpp` also cites the wrong path. **Four** documents now assert a rewrite that never happened — including the ratified register itself. See §7. |
| `threading-model.md:578` | "packet delta encoding \| `T_Worker` \| Planned (Chunk 6+)" | Chunk 6 shipped; delta is explicitly waived as **sim-thread, NOT NetIO** in 11 places. |
| `threading-model.md:577` | "PVS per client \| `T_Worker` \| Planned (Chunk 6+)" | Not built; the PVS state is `s_fatpvs`/`s_fatphs` — **8 KB of file-scope state**, the opposite of per-worker. |

§11 is a 2026-05 forecast table that has outlived its chunks. It reads as
status ("**Today**") and is consulted as truth. It is the most dangerous
document in this packet.

______________________________________________________________________

## 7. Adjudicated sub-decision: unify `assert_main_thread()`

**Verdict: unify onto `assert_thread_role(Main)` and delete
`private/core/assert_main.hpp`.** The "two deliberate notions" reading fails on
four counts:

1. **Scale** — four call sites, all in platform.
2. **Two of the four are already dead** — `console::read_line` has zero
   production callers. The live surface is `crash::install_handler` ×2.
3. **The lazy semantics are strictly worse for the one live site.** The
   predicate short-circuits on an unset id, so the guard on a process-wide
   POSIX `sigaction` install silently no-ops during exactly the startup window
   it exists to protect. `register_thread_role(Main)` is the **first statement
   of `main()`**, so `assert_thread_role` is armed strictly earlier.
4. The doc-drift liability has already fired in three files.

It is a net deletion of a header and a mechanism. Residual cost, stated
honestly: afterwards `crash::install_handler` aborts in any host that never
registers a role — which is the intended behaviour, and the shipped launcher
always registers.

Note the code is the honest party here: `assert_main.hpp` documents its own
no-op window accurately. It is the three *other* documents that assert a
rewrite that never happened.

______________________________________________________________________

## 8. Shape constraints (recorded, not scheduled)

- **When cross-thread cvar reads are eventually built, the read API must be
  value-returning.** No off-main API may return `const char *` or `Cvar *`.
  `cvar_find()`/`cvar_get_list()` stay permanently Main-only:
  `pfnCVarGetPointer` hands a `cvar_t *` to the game DLL, so the
  escaping-pointer shape is **frozen ABI, not a design choice**. Note that
  §8.3's "add a `shared_mutex`" is **wrong as written** — the read path returns
  borrowed pointers into pool memory that the write path `mem_free`s, so a lock
  held for the duration of the call protects nothing.
- **Configuration crossing a thread boundary travels as a snapshot payload on
  the existing MPSC queue**, not by locking the registry. Sound's
  `MixConfigSnapshot` is the shipped precedent.
- **If `T_NetIO` is ever introduced, the flip line is TRANSPORT-vs-DELTA, not
  subsystem-vs-subsystem.** Transport moves; delta stays on the sim thread —
  its own waivers say "NOT NetIO" in 11 places. Loopback stays on `T_Main`
  permanently (§7.2), so singleplayer and multiplayer transport affinity
  diverge, and the HB-2 byte-exact wire witness must be re-derived per
  topology.
- **Before any off-main option is executed**, the 7 fully-plain stats structs
  and the 1 mixed one must be converted to always-on atomics.

______________________________________________________________________

## 9. Re-run triggers

Not a date. Re-derive this packet when any of these becomes true:

1. A **3rd** production `spawn_thread` site appears outside
   `src/sound/topology.cpp` (today: exactly 2).
2. A decision-coupled thread-assert waiver appears **outside `src/networking`**
   — today all 22 are in one subsystem, which is the whole reason this is
   cheap.
3. The networking cvar-read count moves off **zero**.
4. `ThreadRole::Render` or `ThreadRole::NetIO` gains its first real use.
5. `implementation-plan.md`'s Chunk-12 precondition sentence is edited, or
   `threading-model.md` §3.5/§3.7/§11 is revised.
6. A boundary spec is created for any of the six 0-TU skeletons — a renderer or
   physics spec makes option (c) evaluable **for the first time**.
7. Any plain stats struct gains an off-main reader, or `diagnostics_dump`
   lands.
