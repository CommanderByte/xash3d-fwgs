# Threading Model

> **Date**: 2026-05  
> **Scope**: all xash3dpp subsystems — present and future  
> **Status**: decisions made; binding on all new subsystem work  
> **Related**: `design-paradigms-round1.md` (Q-6), `debug-stats-design.md`

---

## 1. Purpose and Scope

This document records the threading model decisions for the xash3dpp rewrite.
It is binding: every new subsystem must be consistent with the thread taxonomy,
communication primitives, and ownership rules defined here, even if it does not
yet use background threads.

The core principle throughout is:

> **Thread safety is an interface property, not an implementation property.**
> A function that takes `const T&` and returns a value is inherently parallelisable.
> A function that writes to shared global state is inherently sequential.
> Writing the right signatures now means enabling parallelism later requires
> removing artificial serialisation, not redesigning synchronisation.

---

## 2. Thread Taxonomy

Six thread roles are defined. A thread registers its role at creation and must
never perform work reserved for a different role.

| ID | Constant | Created by | When started | Notes |
| -- | -------- | --------- | ------------ | ----- |
| `Main` | `ThreadRole::Main` | OS | Process start | The game loop thread. Only thread that calls into game DLLs. |
| `AudioCallback` | `ThreadRole::AudioCallback` | OS audio driver | `Sound::init()` | OS-managed; real-time or high priority. Must never allocate or block. |
| `AudioDecoder` | `ThreadRole::AudioDecoder` | Sound subsystem | `Sound::init()` | Decodes OGG/Opus into PCM ring; feeds `AudioCallback`. |
| `Worker` | `ThreadRole::Worker` | Platform subsystem | `Host::init()` | General job pool — asset loading, PVS, packet delta encoding, HTTP I/O. |
| `Render` | `ThreadRole::Render` | Engine (optional) | `Renderer::init()` | **Deferred to Chunk 10.** See §6. |
| `NetIO` | `ThreadRole::NetIO` | Engine (optional) | `NET::init_thread()` | **Deferred.** See §7. |

The `ThreadRole` enum and its supporting API live in
`xash3dpp/src/platform/thread_role.hpp`:

```cpp
namespace xash::platform {

enum class ThreadRole : uint32_t {
    Unknown = 0,   // before explicit registration
    Main,
    AudioCallback,
    AudioDecoder,
    Worker,
    Render,        // future
    NetIO,         // future
};

// Call once per thread at thread startup.
void register_thread_role(ThreadRole role) noexcept;

// Returns the current thread's registered role.
ThreadRole current_thread_role() noexcept;

// Debug-only assertion — no-op in release builds.
void assert_thread_role(ThreadRole expected) noexcept;

} // namespace xash::platform
```

Internally, `current_thread_role()` reads a `thread_local ThreadRole`. The
existing `assert_main_thread()` helper throughout the codebase is replaced by
`assert_thread_role(ThreadRole::Main)`.

---

## 3. Threading Model — Evolution by Chunk

The model is introduced incrementally. Each chunk may add threads; it must not
break the invariants of the threads already running.

### 3.1 Chunks 1–3 (cmd_cvar, networking, host) — Model A

Single-threaded game loop. The worker pool threads exist (started at
`Host::init()`) but no jobs are submitted to them yet. Audio threads do not
exist (Sound subsystem not yet implemented).

```text
T_MAIN ─────────────────────────────────────────── (all work)
T_WORKER[0..N] ─── (pool idle, no jobs)
```

**Filesystem threading hazards must be fixed before the end of Chunk 3.**
The hazards (`active_game`, `game_loaded`, `gamedir` written in `ActivateGame()`
without a lock; `FindLibrary()` reads game state before acquiring `shared_lock`)
are dormant while only the main thread uses the filesystem. They become live the
moment a worker thread submits an asset load job that calls `FS_Read`. Both sites
must be guarded under the filesystem's existing `shared_mutex` before
`Host::init()` starts the worker pool.

### 3.2 Chunk 4 (world / content) — Model B-partial

The worker pool begins receiving real jobs: asynchronous asset loads (BSP, studio
models, sprites, textures). The `LoadHandle` / `JobToken` pattern (§4) is
introduced here.

```text
T_MAIN ─── game loop, consumes completed load results
T_WORKER[0..N] ─── file I/O + BSP/model/texture parse
```

After this chunk, `WorldData` is immutable once activated. All BSP trace, PVS,
and model query functions take `const WorldData&` — never a mutable global.

### 3.3 Chunk 5 (server) — Model B-partial, dedicated server milestone

No new threads. The server tick runs on `T_MAIN`. Entity thinks (`pfnThink`) are
called sequentially on the main thread — this is a hard constraint from the
GoldSrc game DLL ABI and will not change for the GoldSrc milestone (see §8.1).

Entity state for connected clients can be snapshotted at the start of the server
frame and PVS computation dispatched as worker jobs (one per client) if profiling
shows this is worthwhile. The worker pool is already running; this is an
optimisation, not an architectural change.

### 3.4 Chunk 6 (sound) — Model B (full)

`T_AudioCallback` and `T_AudioDecoder` are introduced by the Sound subsystem.
This is the first chunk where real-time threading constraints apply:

```text
T_MAIN ──── submits play/stop events via sound command queue
T_AudioDecoder ──── reads command queue, decodes OGG/Opus → PCM ring
T_AudioCallback ─── (OS-driven) copies PCM ring → hardware buffer
```

The audio callback must never allocate, block on a mutex, or stall on I/O.
The only shared state between `T_AudioDecoder` and `T_AudioCallback` is the
PCM ring buffer — a lock-free SPSC (single-producer, single-consumer) ring of
pre-decoded 32-bit float frames (see §5.2).

### 3.5 Chunks 7–9 (input, physics, client) — Model B unchanged

No new threads. All subsystems run on `T_MAIN`.

### 3.6 Chunk 10 (renderer) — Model C (optional render thread)

`T_Render` may be added depending on the renderer plugin. See §6.

### 3.7 Deferred — Model D (net I/O thread)

`T_NetIO` is deferred until dedicated server load demonstrates a need. See §7.

---

## 4. Async Work: JobToken Pattern

### 4.1 The problem

The game loop is single-threaded and tick-rate-locked. File I/O and asset parsing
cannot block it. The worker pool runs jobs in the background; the main thread
needs a non-blocking way to check for completion and claim the result.

`std::future<T>` is excluded: its error-reporting path uses exceptions, which
the project disallows (`/EHs-c-`).

### 4.2 JobToken

```cpp
namespace xash {

enum class JobStatus : uint32_t {
    Pending,   // submitted, not yet started
    Running,   // worker thread has begun
    Complete,  // result is ready; caller may move it
    Failed,    // error field is populated
};

template<typename T>
struct JobToken {
    std::atomic<JobStatus>  status{ JobStatus::Pending };
    std::unique_ptr<T>      result;   // valid only when status == Complete
    PathStr                 error;    // valid only when status == Failed
};

} // namespace xash
```

**Worker thread protocol:**

1. `status.store(Running, relaxed)`
2. Perform I/O and parsing; populate a local `T`.
3. On success: move local into `token.result`; then
   `token.status.store(Complete, release)`.
4. On failure: write `token.error`; then
   `token.status.store(Failed, release)`.

The `store(..., release)` in steps 3 and 4 is the only synchronisation needed.

**Main thread protocol (once per frame):**

```cpp
if (token->status.load(std::memory_order_acquire) == JobStatus::Complete)
    activate_world(std::move(token->result));
```

The `load(..., acquire)` pairs with the worker's `store(..., release)`,
guaranteeing that the completed `result` is visible before the main thread reads
it.

### 4.3 Submitting jobs

The worker pool exposes a single entry point:

```cpp
namespace xash::platform {

// Returns a shared_ptr so caller and pool both hold a reference.
// The pool's reference keeps the token alive for the duration of the job.
template<typename T, typename Fn>
std::shared_ptr<JobToken<T>> submit_job(Fn&& fn);

} // namespace xash::platform
```

`fn` is a callable `() -> std::unique_ptr<T>` or `() -> void` that runs on a
worker thread. For void jobs (fire-and-forget), a specialisation is provided.

### 4.4 Ownership on completion

The completed `result` is moved out of the token by the main thread and passed to
the subsystem that requested it. After that move the token may be discarded. There
is no reference back into the worker pool — the pool has released its
`shared_ptr` on job completion.

---

## 5. Cross-Thread Communication Primitives

### 5.1 Summary table

| Boundary | Direction | Primitive | Notes |
| -------- | --------- | --------- | ----- |
| MAIN → WORKER pool | commands | `JobToken` + job queue (mutex + `condition_variable`) | Batch-friendly; worker sleeps when idle |
| WORKER → MAIN | results | `JobToken::status` atomic + `unique_ptr` move | Polled by main once per frame |
| MAIN → AudioDecoder | play/stop events | MPSC command queue (lock-free) | Commands are POD; no allocation in enqueue |
| AudioDecoder → AudioCallback | PCM data | Lock-free SPSC ring buffer | Fixed-size; decoder stalls if full (graceful) |
| MAIN → Render | scene description | Double-buffered `RenderFrame` | See §6.2 |
| Render → MAIN | completion signal | `std::atomic<uint64_t>` frame counter | Main reads to detect stall |
| MAIN ↔ NetIO | packets | MPSC inbound + MPSC outbound queues | Loopback bypasses both; see §7.2 |

### 5.2 PCM ring buffer (AudioDecoder ↔ AudioCallback)

The ring is a fixed-size array of `float` samples (stereo interleaved), sized
for approximately 100 ms of audio at the target sample rate. The decoder owns
the write pointer; the callback owns the read pointer. No atomic CAS is required —
SPSC with sequential producer/consumer is safe with a single `release` store and
`acquire` load on the head/tail indices.

The audio callback copies as many frames as the hardware buffer requires. If the
ring is empty (decoder fell behind), the callback outputs silence and increments
an underrun counter. This counter is always-on (no `XASH_STATS` guard) because
audio underruns are always a bug.

### 5.3 Job queue (MAIN → WORKER pool)

The job queue is a `std::deque<std::function<void()>>` protected by a single
`std::mutex` and a `std::condition_variable`. Worker threads sleep on the
condition variable when idle. This is not a lock-free structure — the queue is
not on a hot path (jobs are submitted at frame start, not per-entity) and the
simplicity is preferable.

The pool size is `min(4, std::thread::hardware_concurrency() - 2)`, clamped to
at least 1. A `dev_worker_threads` cvar overrides this at startup for profiling.

---

## 6. Render Thread (T_Render) — Deferred to Chunk 10

### 6.1 Motivation

The render thread's primary value is **frame overlap**: the GPU executes frame N
while the CPU simulates frame N+1. Without it, any GPU synchronisation point
(fence wait, buffer map, swapchain present) directly delays the game tick. As a
secondary benefit, GPU upload stalls (texture streaming) are the render thread's
problem, not the main thread's.

### 6.2 When to use it

A render thread is only beneficial with an explicit-API renderer (Vulkan,
D3D12). With OpenGL the driver manages the submission queue internally; adding a
game-side render thread gives little benefit and adds coordination complexity
on top of the driver's own threading.

The renderer plugin declares its preference:

```cpp
struct RendererCaps {
    bool wants_render_thread;   // false for GL, true for Vulkan
    // ...
};
```

The engine starts `T_Render` only if `wants_render_thread` is true. The GL
renderer path is permanently single-threaded and that is correct.

### 6.3 The RenderFrame boundary

The interface between `T_Main` and `T_Render` is a double-buffered `RenderFrame`
struct (draw calls, entity transforms, skinned model matrices, light positions,
particle states). The main thread fills `frame[write_idx]` while the render
thread consumes `frame[read_idx]`. At frame boundary, indices swap atomically.

```text
Main:   fill frame[write_idx] → swap ─────┐
Render:                           ← swap  │  consume frame[read_idx] → GPU
```

The renderer plugin exposes a single submission entry point:

```cpp
// Called from T_Render, not from T_Main.
void submit_frame(const RenderFrame& frame);
```

The renderer plugin does not manage threading. The engine drives `T_Render` and
calls into the plugin from it.

### 6.4 Texture uploads

Asset decode (pixel data to CPU memory) runs as a worker job. On `JobToken`
completion, the main thread appends a texture-upload command to the next
`RenderFrame`. The render thread executes the GPU upload via the transfer queue.
The main thread never touches the GPU context.

### 6.5 Building toward the render thread at Chunk 10

The `RenderFrame` struct should be designed before the render thread is started.
Chunk 10 can begin as a direct call from the main thread with the struct already
in place, then move to `T_Render` once the data boundary is verified correct.
This matches the async-loading pattern: design the ownership boundary first;
threading is mechanical once the boundary is clean.

---

## 7. Network I/O Thread (T_NetIO) — Deferred

### 7.1 Current model and why it is adequate

The legacy engine uses non-blocking sockets with `select()` on the main thread.
`NET_GetPacket()` and `NET_SendPacket()` are the only two ingress/egress points.
For the dedicated server milestone (up to ~32 players, standard server hardware)
this is correct and requires no change.

### 7.2 The loopback constraint

The software loopback (`loopback_t`) for singleplayer exchanges client-to-server
packets through shared memory without touching the OS network stack.
`T_NetIO`, if and when introduced, handles only real UDP/TCP file descriptors.
The loopback path always remains on `T_Main`.

### 7.3 Abstraction boundary commitment (now)

The critical decision that must be made now (Chunk 2) is:

> **No `recvfrom()` or `sendto()` calls outside the networking subsystem.**

All packet ingress goes through `NET_GetPacket(netsrc, &addr, &buf)`.
All packet egress goes through `NET_SendPacket(netsrc, addr, buf)`.

This is the only change required now. Adding `T_NetIO` later is then a matter of
replacing the implementation of these two functions to drain/fill queues rather
than call the OS directly. Callers are unaffected.

### 7.4 When T_NetIO becomes justified

Two independent triggers, either of which justifies adding `T_NetIO`:

**Trigger 1 — async DNS / HTTP**: name resolution for master server registration
and HTTP asset downloading block if done synchronously. HTTP socket I/O is the
first candidate for `T_NetIO` work, independent of game packet handling. This can
be phased in during or after Chunk 2.

**Trigger 2 — high player count**: at 64+ players `select()` plus per-packet
main-thread processing starts consuming meaningful frame budget. More importantly,
packets arriving mid-tick are not processed until the next tick. `T_NetIO`
receiving continuously into an inbound queue reduces effective round-trip latency
by up to one server tick.

---

## 8. Hard Constraints — What Will Not Be Parallelised

### 8.1 Entity thinks (pfnThink)

`svgame.dllFuncs.pfnThink(edict)` dispatches into the game DLL. GoldSrc game
DLLs are black boxes: they access global engine state (`gpGlobals`,
`enginefuncs_t` callbacks), call back into the engine (`UTIL_FindEntityByClassname`,
`SV_Move`, etc.), and modify any entity at any time. This is fundamentally not
re-entrant. Entity thinks are sequentially dispatched on `T_Main` and will
remain so for any GoldSrc-compatible game DLL.

A custom game DLL API (out of scope for the current project) could make this
parallelisable. The engine should not create obstacles to such an API in the
future. The design rule that helps: engine functions called from thinks should
accept explicit context parameters, not access globals. This is good practice
regardless; it also means a future think sandbox can intercept the context.

### 8.2 Player physics (pm_shared)

`PM_Move` uses a single global `pmove_t` (`svgame.pmove`). Players are processed
sequentially. Making this parallel requires one `pmove_t` per player and breaking
the global-state dependency — this restructures a frozen ABI surface. Deferred
past the GoldSrc milestone.

If the physics subsystem (Chunk 8) is designed with a per-body `PhysicsContext`
struct, the pm_shared path can be wrapped inside it and the global replaced when
the time comes. The chunk design should not introduce new global physics state.

### 8.3 Cmd/cvar

The command buffer and cvar write operations are main-thread-only. Cvar reads
from worker threads are safe (the `shared_mutex` in `CmdCvarContext` permits
concurrent readers). No change required.

### 8.4 Client DLL (cdll_int.h)

Same constraint as the game DLL: the client DLL ABI is not re-entrant. All
client DLL calls happen on `T_Main`.

---

## 9. Interface Design Rules for Thread Safety

These rules apply to all subsystems. They are the minimum needed to keep future
threading options open without requiring a redesign.

### Rule 1 — Query functions take const context parameters

Functions that read world state, entity state, or BSP data must take `const T&`
parameters, never read from a global:

```cpp
// Correct — safe for concurrent read access
bool trace_line(const WorldData& world, vec3 start, vec3 end, TraceResult& out);

// Wrong — reads global, cannot be called concurrently with any world mutation
bool trace_line(vec3 start, vec3 end, TraceResult& out);  // reads g_world internally
```

After `WorldData` is activated (§4.4), it is immutable. All trace, PVS, and
model queries become safe for concurrent reads by design.

### Rule 2 — Mutation is explicit at the call site

Any function that modifies shared state must make the mutation visible in its
signature (non-const reference parameter, or a return value that the caller
commits). Avoid hidden mutation through globals or thread-local side channels.

```cpp
// Correct
void apply_entity_state(EntityList& entities, EntityId id, const EntityDelta& delta);

// Wrong — applies changes via side effect to a global list
void apply_entity_state(EntityId id, const EntityDelta& delta);
```

### Rule 3 — Compute and commit are separate phases

Within the server tick, reads precede writes. Collect all inputs first; write
results back second. Even when execution is sequential, this makes the read set
and write set visible in code structure, which is the prerequisite for converting
the compute phase into parallel jobs later.

### Rule 4 — No new global mutable state

New subsystems must not introduce mutable global variables. State lives in a
context object (pimpl class or explicit `Context&` parameter). The only
exceptions are thread-local variables that are private to the threading
infrastructure itself (e.g. `g_current_role`).

### Rule 5 — Document thread-safety on public APIs

Every public function in a new subsystem header must be annotated with one of:

```cpp
// @thread-safety: main-thread-only
// @thread-safety: any thread (const read on immutable data)
// @thread-safety: any thread (internally synchronised)
// @thread-safety: worker threads only
```

This is enforced by the reviewer checklist, not the compiler. The annotation
prevents the slow drift where "probably safe" assumptions accumulate unexamined.

---

## 10. Filesystem Threading Hazards — Required Fix Before Chunk 3

Two hazards in `xash3dpp/src/filesystem/filesystem.cpp` (the `Impl` class) must
be resolved before `Host::init()` starts the worker pool.

**Hazard 1**: `Impl::active_game`, `Impl::game_loaded`, and `Impl::gamedir` are
written in `ActivateGame()` without holding any lock. They are read in `Rescan()`
and `FindLibrary()`. A worker thread calling `FS_Open()` during a game activation
(e.g. async content pre-load triggered by a changelevel command) creates a data
race.

**Fix**: acquire the exclusive lock (`unique_lock` on the filesystem `shared_mutex`)
at the start of `ActivateGame()` for the full duration of the game-state field
writes.

**Hazard 2**: `FindLibrary()` reads `active_game` and `gamedir` before acquiring
the `shared_lock` on the search path list. The load-bearing read is not protected.

**Fix**: move the `shared_lock` acquisition to the top of `FindLibrary()`, before
any field reads.

Both hazards are marked with `// THREADING HAZARD` comments in the source today.
They become active bugs — not latent ones — when the first worker thread calls
into the filesystem.

---

## 11. Per-Subsystem Thread Assignment Reference

| Subsystem | Thread | Constraint source |
| --------- | ------ | ----------------- |
| cmd/cvar — write | `T_Main` | Command buffer is single-consumer |
| cmd/cvar — cvar read | Any | `shared_mutex` in `CmdCvarContext` |
| filesystem | `T_Main` for writes; any for reads after lock | See §10 |
| memory — allocate | Any (pool spinlock) | Documented in memory subsystem |
| platform | Any | Pure functions or internally synchronised |
| host loop | `T_Main` | Drives the frame |
| server tick, entity thinks | `T_Main` | GoldSrc game DLL ABI (§8.1) |
| client tick | `T_Main` | Client DLL ABI (§8.4) |
| physics (pm_shared) | `T_Main` | Global pmove state (§8.2) |
| input (SDL event pump) | `T_Main` | SDL2 event API is not thread-safe |
| world queries (after load) | Any | `const WorldData&` — immutable after activate |
| asset loading | `T_Worker` | Worker job via `JobToken` |
| PVS per client (optional) | `T_Worker` | Entity state snapshot taken on main first |
| packet delta encoding | `T_Worker` | Per-client, independent |
| HTTP I/O | `T_Worker` → `T_NetIO` | Worker now; migrate to NetIO thread later |
| sound command queue write | `T_Main` | MPSC enqueue |
| sound decoding | `T_AudioDecoder` | |
| sound playback | `T_AudioCallback` | OS real-time callback |
| GPU submission | `T_Render` (if enabled) | Renderer plugin called from render thread |
| GPU upload | `T_Render` | Via `RenderFrame` texture upload commands |
| save / demo / UI | `T_Main` | No parallelism benefit; state is sequential |
