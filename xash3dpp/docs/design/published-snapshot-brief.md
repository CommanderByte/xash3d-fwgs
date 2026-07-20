# Design brief — HB-5, the published-snapshot idiom (P-2)

> **Date**: 2026-07-20 (tree-wide modernization audit, lens L1)\
> **Status**: **ANALYSIS DISCHARGED, PRIMITIVE GATED.** The verdict is *no
> shared primitive is warranted today*. What ships instead is a naming rule
> and a shape-constraint register. The remaining half is gated on the
> Chunk-12 thread-model decision.\
> **Register**: candidate Q entry for the vocabulary rule (§3).\
> **Related**: `thread-model-decision-packet.md`, `threading-model.md` §5/§6.3,
> `extension-goals.md` P-2 and §6

______________________________________________________________________

## 1. Why the answer is "not yet"

HB-5 asks for "one shared published-snapshot idiom". The premise is that the
tree has several competing mechanisms. It does not — it has **exactly one**
cross-thread structured publisher, and that one has **zero production
consumers**.

The bounding fact: **two production thread spawns exist in the entire tree,
both in `src/sound/topology.cpp`.** A "publish mechanism" can only genuinely
cross a thread boundary if one end of it is the decoder or the callback
thread.

| Tier | Count | What |
|---|---|---|
| **A — genuinely crosses a thread boundary, structured payload** | **1** | sound's `publish_channels_if_requested`/`channel_snapshot`: a request-flag + mutex + release-counter handshake with a bounded spin. Its consumer, `Sound::channels_snapshot()`, has **zero production callers** — tests only. |
| **B — crosses a boundary, but scalar counters** | 7 | `SoundStats`, the epoch ack, packed mouth slots, `NetworkingStats`/`DeltaStats`, `CmdCvarStats`, `ServerStats`, the pool-registry CAS, the engine-context pointer swap. A counter block and a pointer swap are already the right shapes; none of these wants `Published<T>`. |
| **C — structured cross-thread data with no snapshot at all** | 1 | `LockedSfxResolver`/`SfxRegistry`: mutex-on-lookup **plus a hard address-stability invariant** (reserve-to-max at construction, refuse to grow, never free). The decoder borrows Main-owned buffers with zero copying. **This is the tree's empirically successful idiom and HB-5 does not mention it.** |
| **D — queue/ring transport** | 3 | The MPSC command queue and the PCM ring. Transport, not publication: a queued POD is consumed once; a published snapshot is read repeatedly. |
| **E — Main-only, wearing snapshot vocabulary** | 10 | input's bindings/touch/OSK/joy snapshots, content's `model_infos()`, map_loader's world swap, save's provider spans, cmd_cvar's descs, `filesystem::stats()`, `Host::stats()`, `Clock::stats()`. **Zero thread crossings.** |

Building `core::Published<T>` now would serve one mechanism that has no
consumer, and would be the fourteenth zero-implementation abstraction in a
tree that already carries thirteen — added by an audit whose stated purpose is
to stop exactly that.

### Corrections to HB-5's own citations

Three of the four references the backlog entry rests on do not say what it
claims:

- **map_loader's "atomic world swap"** is qualified *in the same sentence* as
  Main-only and non-overlapping. Not a cross-thread publisher. (A Phase-1
  finding claiming otherwise was **refuted**.)
- **server's `snapshot_*` family** is the **wire entity-delta pipeline** —
  baselines, the packet-entities ring, per-client frame rings — consumed by
  netchan on Main. Server's only cross-thread surface is `ServerStats`. Two
  independent packs mis-read this as P-2 evidence *because of the entry's
  wording*.
- **`RenderFrame`**, the doc's named "P-2 reference implementation", is **zero
  code**, and is gated behind Chunk 13's unresolved backend precondition.

And one correction in the opposite direction: `Cvar::generation` was described
as "a generation counter with no publisher". It is backwards — it has **two
release publishers and zero consumers**. No `.load()` on it exists anywhere.

______________________________________________________________________

## 2. What ships now: the vocabulary rule

This is worth more than the primitive and costs almost nothing.

**"Snapshot" currently means at least three unrelated things.** Of 308
occurrences tree-wide, **165 (54%) are in `server` and mean network entity
delta**. A reader cannot tell from a symbol name which concept they are
holding. That ambiguity is the root cause of the misreadings above, and it
will recur in Chunk 12 when client work meets the delta pipeline.

Three reserved terms:

| Term | Means | Legal today in |
|---|---|---|
| `publish` / `published_*` | A **cross-thread** one-writer/many-reader handoff, and only where a second thread genuinely exists on that path. Using it Main-only is a naming defect. | `sound/topology`, `memory/pool_registry`, `host/engine_context_accessor` |
| `snapshot` | An **owned, caller-consumable copy** of state at an instant (P-4 value types). Says nothing about threads. | `CvarDesc`, `model_infos()`, `bindings_snapshot()`, `PoolStats` |
| `delta_frame` / `baseline` | The **wire** entity pipeline. | `server/clients` |

**Corollary**: an accessor returning `const Stats &` into live `Impl` state is
**never** a snapshot, whatever it is named. The conforming precedent is
`MapLoader::stats()`, which returns by value. `Clock::stats()` and
`Host::stats()` are the violations.

The `server` rename (`snapshot_*` → `delta_frame_*`, ~165 sites) is recorded
as a **Chunk-12 precondition, not work now**: it is a pure ambiguity deletion
with no behavioural content, but it sits beside the HB-2-fenced wire bit-codec
and delta field widths. Doing it standalone is churn with parity-review cost
and no consumer; doing it when Chunk 12 already has those files open is nearly
free.

______________________________________________________________________

## 3. Shape constraints — binding when the primitive is eventually built

Recorded so that whoever builds it (earliest: Chunk 13's `RenderFrame`) cannot
get it wrong. Several are non-obvious.

1. **It lives in `core/` as the third member of the existing family.**
   `mpsc_queue.hpp` and `spsc_ring.hpp` set the house style: template on the
   payload, `static_assert(std::is_trivially_copyable_v<T>)`, fixed
   compile-time capacity, zero allocation after construction, explicitly
   64-bit cursors, `static_assert(std::atomic<pos_t>::is_always_lock_free)`.
   Anything else is a foreign body.
2. **No atomic wider than 64 bits.** x86 ships and there is no portable DWCAS.
   That rules out a tagged `{pointer, sequence}` swap and points at
   seqlock-over-POD or double-buffer-with-atomic-index.
3. **Publication is demand-driven (request/ack pull), not per-frame push.** The
   one working in-tree mechanism publishes only when a reader asks, spins a
   bounded number of yields, and falls back to the last published value. A
   publish-every-frame double buffer puts cold introspection cost on the hot
   path.
4. **Two explicit tiers, not one.** POD payloads ride the lock-free primitive;
   string-bearing payloads are copied out under a lock. This is already what
   sound and filesystem do. A single tier attempting to cover both will fail.
5. **P-2 relaxes the READ side only.** Writer and lifecycle Main-pinning
   survives a published-snapshot design — single-publisher discipline *is* the
   mechanism. Content's twelve Main asserts are all writers/lifecycle and are
   permanent under P-2. What is missing there is a *read-side* assert.
6. **Never the vehicle for the wire entity-delta pipeline.** That is Main-only,
   netchan-consumed, and sits beside a fenced kernel.
7. **A double buffer wanting `alignas(64)` false-sharing separation cannot be
   `pool_new`'d** — the pools assert `alignof(T) <= 8`. Either the primitive
   stays 8-byte-alignable or HB-7 lands first. (This gives HB-7 a second,
   concrete requirer; see `allocation-seam-brief.md`.)
8. **Evaluate the address-stability borrow as an alternative, not merely as
   prior art.** `LockedSfxResolver` gets structured Main-owned data to a worker
   thread with **zero copying**, and it works in production today. A brief that
   starts from "which snapshot shape?" has already skipped the cheaper answer.

______________________________________________________________________

## 4. The gate

HB-5's central question — *is a shared publish primitive warranted?* — is
decidable only once it is known whether a second long-lived thread will ever
exist outside `src/sound/topology.cpp`. That is exactly the Chunk-12
thread-model decision, and (for the render half) the Chunk-13 backend
decision.

**Record HB-5 as gated on those two decisions, not as open-and-unstarted.**
Today it reads as overdue buildable work, which invites someone to discharge
it speculatively. A backlog item whose answer is *"no, here is why, and here
is what to do instead"* is **discharged, not abandoned**.

______________________________________________________________________

## 5. Non-goals

No `core::Published<T>`. No retrofit of the ten Main-only surfaces. No rename
of server's delta pipeline today. No wiring of `channels_snapshot()` to a
consumer — the obvious candidate would be building a debug overlay to justify
a mechanism, which is the anti-gold-plating failure inverted.

______________________________________________________________________

## 6. Re-run trigger

Re-derive this brief when a **second** genuine cross-thread structured
publisher appears — i.e. when a third production `spawn_thread` site lands, or
when `RenderFrame` acquires code. Not on a date.
