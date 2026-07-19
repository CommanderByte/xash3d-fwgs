# Design brief — thread spawn, ThreadRole registration, and the P-1 inbox

> **Status**: delivered 2026-07-19 (chunks-8/9/10 campaign B2; discharges the
> HB-4 design-brief gate). Register entry: Q-24 (QUEUE_FAMILY_HOME).
> Cross-subsystem: `core` / `platform` / `host` / `sound` (first consumer).

## 1. Why now

No thread has ever been spawned in the xash3dpp tree (no `std::jthread`
outside tests; the worker pool is unscheduled). Chunk 9's `T_AudioDecoder`
is the first off-main consumer — exactly the trigger the platform boundary's
door-keep names ("add a thin thread-spawn wrapper that registers a
`ThreadRole` on entry when the first off-main consumer is scheduled") and
the condition HB-4 gates behind this brief. Every north-star off-main thread
(G-1 MCP listener, G-3 debug thread, future NetIO, the P-1 worker pool)
rides the same two primitives designed here.

## 2. The split (Q-24)

| Piece | Home | Why |
|---|---|---|
| `MpscQueue<T>` (typed MPSC command queue) | `core` | Portable atomics, no OS surface; beside `ThreadRole`/`Clock`. Q-11 inapplicable (primitive, not satellite). |
| `SpscRing<T>` (fixed-size single-producer/single-consumer ring) | `core` | Same reasoning; sound's PCM ring is the first instantiation, NetIO-class consumers later. |
| `spawn_thread(role, name, priority, fn)` | `platform` | OS boilerplate: thread creation, `SetThreadDescription`/`pthread_setname_np` naming, `SetThreadPriority`/`sched` priority. Registers the `ThreadRole` on entry before `fn` runs. |
| Main-inbox drain slot (host `RunFrame`, at the `cbuf_execute` mark) | `host` | **Designed, NOT built** — no consumer exists yet (extension-goals §6: primitives land with their first scheduled consumer). |

## 3. Contracts

### 3.1 `core::MpscQueue<T>`

- `T` must be trivially copyable POD (`static_assert`); capacity is a
  compile-time/init-time constant from `limits.hpp`.
- **No allocation in enqueue** (threading-model §5.1) — fixed slot array;
  proof = trivially-copyable static_assert + a pool-counter-delta test.
- Multi-producer safe (CAS ticket or equivalent), single consumer.
- Full-queue behaviour is POLICY-PARAMETERIZED by the consumer subsystem:
  sound's policy is SND-OQ-3 (reserved STOP/CHANGE fast lane; START
  overflow briefly blocks the producer, never drops). The primitive
  exposes `try_push` + `push_reserved_class` so policies compose without
  forking the queue.
- Genericity pin: core's own test suite instantiates a NON-audio message
  type; no audio type may appear in `core`.

### 3.2 `core::SpscRing<T>`

- Fixed-size, one release-store/acquire-load pair on head/tail (threading-
  model §5.2); producer stalls gracefully when full; consumer reads
  whatever is available (empty ⇒ caller-defined silence path).
- Occupancy accessor is part of the type (the G-3 read pattern) — no bare
  `volatile` peeks (retires the legacy `s_rawend` idiom).
- Sound instantiates `SpscRing<int16_t>`-family frames (SND-OQ-5); framing
  policy (watermarks, underrun counter — always-on per §5.2) stays in the
  consuming subsystem.

### 3.3 `platform::spawn_thread`

- Signature shape: `spawn_thread(core::ThreadRole role, const char *name,
  ThreadPriority prio, Fn &&fn) -> JoinHandle` (exact C++ shape finalized
  at S9.0 implementation; `JoinHandle` is join-on-destruction RAII).
- Registers `role` via `register_thread_role` as the FIRST action on the
  new thread; the assert machinery fires in every build (thread_role.hpp
  as-built; threading-model doc corrected 2026-07-19).
- `ThreadPriority` is declared now ({Normal, High, Realtime}); Realtime
  semantics may stub (log + Normal) until the SDL audio device chunk needs
  it — recorded so `src/sound` never grows inline `SetThreadPriority`
  boilerplate.
- Boundary updates owed at S9.0: platform-boundary Interface/Threading/
  Extension-axes rows + §3a.

### 3.4 Main-inbox drain slot (designed, not built)

The future P-1 service inbox is `core::MpscQueue<ServiceMsg>` drained by
host `RunFrame` at the existing `cbuf_execute` mark, once per frame, before
command-buffer execution — so marshalled mutations (G-1 MCP actions, G-3
debug commands) observe the same ordering as console input. Nothing is
built until the first such consumer is scheduled; sound's audio queue is
NOT this inbox (opposite direction: Main produces, worker consumes) and
does not discharge it.

## 4. First consumer & validation

Chunk 9: S9.0 lands `spawn_thread` (platform); S9.7a lands the queue family
(core, own green commit); S9.7b wires sound's topology through both. Tests:
MPSC multi-producer stress, no-alloc-enqueue proof, ring-full stall,
empty-ring underrun, stop-and-free race (SND-OQ-2 epoch fence), soak under
debug asserts, both arches. JobToken/worker pool accedes to the family when
scheduled (extension-goals §4 Chunk-7 status note).

## 5. Non-goals

No worker pool, no NetIO thread, no debug thread, no MCP listener, no
`dev_worker_threads` cvar — each lands with its own consumer. No scheduler
or affinity surface. No cross-process anything.
