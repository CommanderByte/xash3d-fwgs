# Debug and Stats Infrastructure — Design Analysis

> **Date**: 2026-05  
> **Scope**: cross-cutting; all xash3dpp subsystems  
> **Status**: analysis + recommendations; no code changed

---

## 1. What Exists Today

### 1.1 Legacy engine (reference)

The legacy engine grew its debug/stats infrastructure organically over 20+ years.
Every category of information uses a different, incompatible mechanism:

| Category | Mechanism | Output |
|----------|-----------|--------|
| Developer verbosity | `host_developer` cvar (0–2) | Console text |
| Network packet trace | `net_showpackets`, `net_showdrop` cvars | Console text |
| Visual network graph | `net_graph` cvar + `netstat_*` ring buffers | Rendered overlay |
| FPS / position | `cl_showfps`, `cl_showpos` cvars | Rendered overlay |
| Message tracing | `cl_trace_messages`, `sv_trace_messages` | Console text |
| Memory accounting | `memlist` command | Console text |
| Renderer perf | `r_speeds` cvar → renderer DLL callback | Rendered overlay |
| Remote console | RCON (UDP, password-auth) | Network → console |
| Crash dump | Windows SEH → dialog | Dialog / file |

**Recurring problems with the legacy approach:**

- Stats are *text* — no structure, impossible to query or graph externally.
- No separation between measurement and reporting; the cvar both enables the counter
  and controls the text output.
- RCON is the only external channel and it only carries console text.
- Compile-time guards are sparse and inconsistent (`#ifdef _DEBUG` in some files,
  absent entirely in others).

---

### 1.2 xash3dpp rewrite (current state)

The rewrite established a two-tier compile-time model in the cmd_cvar subsystem that
generalises well:

| Tier | Guard | Cost | What it covers |
|------|-------|------|---------------|
| Lightweight | `XASH_STATS` | ~1 atomic inc per event | Per-cvar `write_count`, command dispatch counters (`commands_executed`, `commands_dropped`), peak registration counts |
| Heavy tracing | `XASH_DEBUG_CVARS` | Circular buffer write | 256-entry cvar change log (old/new value, frame, source), break-on-write, hash-bucket histograms |

Memory subsystem uses **always-on** atomics in the pool registry (relaxed memory order,
measured as negligible overhead). The pool stats API (`get_stats`, `for_each_pool`)
provides a clean structured read interface.

**Output channels (codified rule):**

| Channel | Use for |
|---------|---------|
| `core::log(LogLevel::Warning\|Error\|Fatal, tag, msg)` | C++ subsystem diagnostics — anything a developer would want in a crash log or CI failure trace. Always reaches the test-capturable callback. |
| `platform::console::write(std::string_view)` | Game console output — user-facing commands (`echo`, `cvarlist`, `memlist`, stats dumps) and runtime stats reporting. |
| `core::log(LogLevel::Info, tag, msg)` | Pre-console / early-init only — demoted; do not use for normal subsystem chatter. |

The two channels are intentionally separated: the log path is *diagnostic* (test
callback can capture, severity-tagged, structured), while `console::write` is the
*game console* (raw bytes, no formatting). Stats reporting writes structured
records to `console::write`; subsystem errors that matter for debugging write
to `core::log`.

**Gap summary:**

- Tier 1 (`XASH_STATS`) and Tier 2 (`XASH_DEBUG_CVARS`) exist only in cmd_cvar.
  No other subsystem has defined its stats struct yet.
- There is no runtime on/off switch for stats output. Everything is compile-time.
- There is no external (network) channel.
- There is no shared stats-reporting contract that subsystems can implement uniformly.

---

## 2. Design Goals

Before choosing mechanisms, fix the goals:

1. **Zero overhead when not needed** — release builds must not pay for debug
   infrastructure that is never read.
2. **Non-blocking measurement** — counters must not block the main loop or game
   threads, regardless of whether any consumer is attached.
3. **Separation of measurement and reporting** — increment a counter cheaply;
   decide at runtime where (or whether) to send the data.
4. **Structured data** — export typed key/value pairs, not ad-hoc console text,
   so external tools can parse and graph them without screen-scraping.
5. **External visibility** — attach an external tool to a running instance without
   restarting or recompiling.
6. **Incremental** — the infrastructure must not require all subsystems to be
   finished before anything works.

---

## 3. Compile-Time vs Runtime — Detailed Analysis

### 3.1 Compile-time gating (the `#if XASH_STATS` layer)

**What it does well:**

- Zero overhead in release builds; the optimizer eliminates dead branches and unused
  struct fields entirely.
- Enables aggressive inlining of counter increments (`+= 1` on an `atomic<uint64_t>`
  with `relaxed` order costs ~1 ns on x86-64; a release build without the guard costs
  0 ns).
- Makes the debug contract visible in the type system: a struct defined under
  `#if XASH_STATS` cannot be accidentally used in production paths.
- Reproducible: the same binary always has or always lacks the counters; there is no
  "I enabled it and it slowed down" surprise in production.

**What it cannot do:**

- Cannot diagnose production issues without a rebuild.
- Cannot be selectively enabled for one subsystem while leaving others off in the same
  binary.
- Cannot be toggled by an operator without stopping the server.

**Verdict**: compile-time guards are correct for Tier 2 (heavy tracing — change logs,
break-on-write, histograms) and for any counters whose measurement cost is not
negligible. They are **not** the right tool for the primary stats data that operators
need to inspect in production.

---

### 3.2 Always-on lightweight counters (what the memory subsystem does)

Atomic `relaxed` increment on allocation: on a modern x86-64 core a `relaxed` 64-bit
atomic increment is ~1 ns, indistinguishable from a non-atomic increment in
microbenchmarks. The main-loop cost is zero on the critical path because allocations
are not on the critical path.

**This is the right model for production stats that must always be readable:**

- Pool live bytes, total allocs, total frees: always-on in memory.
- Future: command dispatch count, cvar write count — these should also be always-on
  (they are cheap).
- NOT always-on: 256-entry circular change log, per-frame histograms, break-on-write
  — these have real cost and belong in Tier 2.

**Recommendation**: promote `XASH_STATS` counters that cost ≤ 1 atomic per event
to "always-on" (no guard). Reserve `XASH_STATS` for counters that require more
bookkeeping (e.g. tracking peak values, computing moving averages) and
`XASH_DEBUG_*` for everything that touches memory proportional to event count.

---

### 3.3 Runtime gating

Runtime gating means "the counter was measured; decide now whether to emit it."
Two distinct cases:

**Case A — runtime emission gate (cheap)**

A single atomic boolean or pointer that the reporter checks before serializing.
If no consumer is connected, the hot path is: load atomic → branch-not-taken → done.
Cost: ~1 ns. Correct for all tiers.

**Case B — runtime measurement gate (avoid)**

Putting an `if (debug_enabled)` in front of the counter increment itself. This is what
the legacy engine does (`if (net_showpackets.value) Con_Printf(...)`). Problems:
- The `if` is not optimised away; it stays in the binary.
- Measuring *and* reporting are coupled; you cannot read the counter unless the cvar
  is on.
- Branching inside hot paths can cause branch mispredictions under infrequent enable.

**Conclusion**: measure always (or at compile-time tier boundary); gate *output* at
runtime.

---

## 4. External Inspection Channel Design

### 4.1 Protocol choice: UDP stats stream + TCP command channel

Two separate channels serve two distinct needs:

| Channel | Protocol | Why |
|---------|----------|-----|
| Stats stream | UDP | Fire-and-forget; no backpressure; drops are acceptable (stale metrics are fine); ~0 impact on sender if no consumer is present |
| Command console | TCP | Reliable; ordered; bidirectional; maps naturally to the existing RCON model |

Both listen on loopback only by default (`127.0.0.1`). A config option promotes to
all interfaces for remote tooling.

### 4.2 Stats stream architecture (UDP)

```
[Main loop / subsystem threads]
        │  atomic reads (zero alloc)
        ▼
[Stats collector]  ── ring buffer (lock-free) ──►  [Serializer thread]
                                                           │
                                                      UDP socket
                                                           │
                                                    [External tool]
```

**Ring buffer**: fixed-size (e.g. 1024 slots), each slot is a `StatsSample`:

```cpp
struct StatsSample {
    std::uint64_t  timestamp_us;   // from platform::get_time()
    std::uint32_t  subsystem_id;   // enum, registered at startup
    std::uint32_t  metric_id;      // per-subsystem metric number
    std::uint64_t  value;          // counter value or packed float
};
```

**Hot path** (called from any thread):
1. Atomic read of the counter (already exists).
2. `ring.try_push(sample)` — one CAS on the write index. If the buffer is full,
   the sample is dropped silently (stale metric, not a correctness issue).

**Background serializer thread** (low priority):
1. Drain ring buffer.
2. Encode samples as msgpack or a trivial binary frame (4-byte length + payload).
3. `sendto` on UDP socket. If no consumer, the OS drops the packet immediately.

**Cost when no consumer is attached**: the ring push still happens (~5 ns), but the
serializer thread drains it immediately into a no-op send (OS drops in kernel). This
is the only overhead. A `std::atomic<bool> has_consumer` read before the ring push
can eliminate even that.

### 4.3 Command channel architecture (TCP)

Maps to the existing RCON design. A single TCP server socket accepts one connection
at a time. The protocol is line-oriented: the client sends a command (UTF-8, newline
terminated); the server routes it through `CmdCvarContext::cbuf_add_text` on the next
frame and sends back captured console output.

This does not require a separate thread for the main-loop integration; a non-blocking
`accept`/`recv` poll at the start of each frame is sufficient for a single-connection
command interface.

### 4.4 Wire format recommendation

For the UDP stats stream, a minimal self-describing binary frame is preferable to JSON:

```
[4 bytes: magic 0x58535453 "XSTS"]
[4 bytes: frame length]
[8 bytes: server uptime us]
[4 bytes: sample count N]
[N × 24 bytes: StatsSample records]
```

This is trivially parseable in Python/Rust/Go for tooling, and emitting it requires no
heap allocation on the engine side (serialize directly from the stack into a fixed
`uint8_t buf[4096]`).

---

## 5. Performance Analysis

### 5.1 Counter measurement overhead

| Operation | Cost (x86-64) | Notes |
|-----------|--------------|-------|
| `relaxed` atomic increment | ~1 ns | Compiles to `lock xadd`; no memory barrier |
| `relaxed` atomic load | < 1 ns | Register read on TSO |
| Ring buffer push (CAS) | 2–5 ns | One CAS on write index; contention-free in normal use |
| `try_push` check + skip | ~1 ns | Single branch + atomic load |

For a 100 Hz main loop, 1 ns = 0.01 µs = 0.0001% of a 10 ms frame budget. The
measurement tier has **no measurable impact on frame rate**.

### 5.2 Serializer thread overhead

The serializer thread runs at SCHED_IDLE / `THREAD_PRIORITY_IDLE`. It wakes when
the ring buffer has data and there is a consumer socket. On a 100 Hz server with 20
active metrics, the serializer produces ~2000 samples/s × 24 bytes = ~50 KB/s of
UDP traffic. At loopback speeds this is negligible.

The serializer **never touches main-loop data structures**; it only reads from the
ring buffer that the main loop writes to. There is no lock between them.

### 5.3 Tier 2 (heavy tracing) overhead

The 256-entry cvar change log writes ~120 bytes per change (name + old value + new
value + metadata). During steady-state gameplay, cvars rarely change; the log overhead
is effectively zero. During a `exec config.cfg` at startup, hundreds of writes may
occur, but startup is not frame-rate-sensitive.

The hash-bucket histogram requires iterating all buckets (O(B) where B = 64 for cvars
by default). Called only from `dump_hash_stats`, which is a debug command, not a hot
path.

Break-on-write: one `strcmp` per cvar write when enabled. Cvar writes are rare. Zero
cost when the watched name is `nullptr`.

**Conclusion**: Tier 2 is safe even in development builds; it has no frame-rate impact
during normal gameplay.

### 5.4 The one real risk: string formatting

The legacy engine's biggest debug performance mistake is `Con_Printf` inside hot loops
(`net_showpackets` called per-packet, `r_speeds` formatted every frame). These are slow
because:
- `vsnprintf` → stack allocation → cache pollution.
- Console write → potential lock on the console buffer.
- Formatted string is discarded immediately if nothing reads it.

**Rule for the rewrite**: never format strings on the hot path. Counters accumulate
raw numeric values; the serializer thread (or a query command) formats them on demand.

---

## 6. Recommendations

### 6.1 Immediate: standardise the stats struct pattern (no new code needed)

Every subsystem that has a non-trivial hot path should define a `<Subsystem>Stats`
struct following the memory subsystem model:

```
xash3dpp/include/xash3dpp/<subsystem>/stats.hpp   (or inline in context.hpp)
```

Fields:
- Always-on (no guard): counters that cost ≤ 1 atomic per event and are
  potentially useful in a profiling build (memory bytes, command counts, etc.).
- `#if XASH_STATS`: counters requiring more bookkeeping (peak tracking, high-water
  marks, etc.).
- `#if XASH_DEBUG_<SUBSYSTEM>`: heavy tracing (ring buffers, value history, etc.).

Access: a `const Stats &stats() const noexcept` method on the context class, mirroring
what `CmdCvarContext::stats()` already provides.

The memory subsystem (`for_each_pool` + `get_stats`) is the template to follow.

### 6.2 Near-term: runtime emission gate (single struct, no network yet)

Before the external channel exists, add a single place where all stats are aggregated
and emitted on demand:

```cpp
// In platform/ or a new diagnostics/ subsystem:
void diagnostics_dump(CmdCvarContext &, MemorySubsystem &, ...) noexcept;
```

Called by a `stats` built-in command (or `dump_stats` for dev builds). This is the
console-text version; it prepares for the structured version by forcing all stats to
flow through one function rather than each subsystem writing directly to console.

### 6.3 Medium-term: external channel

Implement the UDP stats stream and TCP command channel described in §4 as a
`diagnostics` subsystem:

```
xash3dpp/include/xash3dpp/diagnostics/
    channel.hpp       — StatsChannel: open/close/push
    stats_frame.hpp   — StatsSample, wire frame layout
xash3dpp/src/diagnostics/
    channel.cpp       — ring buffer + serializer thread + socket
```

The channel is optional at link time (feature flag in CMake). When not linked,
`channel.hpp` provides no-op stubs. No existing code needs ifdefs; it calls
`channel.push(sample)` which is a no-op stub in non-diagnostics builds.

### 6.4 Harmonization summary

| Item | Action | Priority |
|------|--------|----------|
| Promote cheap `XASH_STATS` counters to always-on | Edit `cvar.hpp`, `context.hpp` | Low (cosmetic) |
| Define `SubsystemStats` for memory (already done), cmd_cvar (done), filesystem (pending) | Add `FilesystemStats` when filesystem subsystem is written | Deferred |
| `diagnostics_dump` aggregator command | Add when ≥ 3 subsystems have stats structs | Medium |
| `diagnostics` channel (UDP + TCP) | New subsystem | After host layer is drafted |
| Remove compile-time guard from `dump_hash_stats` | Add an explicit `[[maybe_unused]]` attribute or keep `XASH_DEBUG_CVARS` guard | Low |

### 6.5 What NOT to do

- **Do not replicate `host_developer`** — a single integer cvar that gates half the
  debug output is an anti-pattern. Per-subsystem independent flags are better.
- **Do not gate counter increments on a runtime bool** — always measure, gate the
  output.
- **Do not format stats strings in hot paths** — accumulate raw values; format in the
  serializer or query handler.
- **Do not use TCP for the stats stream** — backpressure on TCP can stall the
  serializer thread and introduce latency into the main loop via any shared state.
- **Do not open the external channel on non-loopback addresses by default** — this is
  a security boundary; RCON-style remote access should require explicit opt-in.

---

## 7. Open Questions

1. **Frame number source**: `last_write_frame` in `CvarWriteSource` uses a `uint32_t`
   frame counter. Who owns the authoritative frame counter, and how do subsystems
   read it without coupling to the host layer? Consider a `diagnostics::current_frame()`
   free function backed by a relaxed atomic updated by the host loop.

2. **Consumer authentication for the TCP channel**: RCON uses a shared password.
   For a local developer tool, a single-use token generated at startup and printed to
   the console (visible only to the local operator) is simpler and more secure.

3. **Metric registration vs hardcoded IDs**: the `StatsSample::metric_id` field
   requires either a central registry or per-subsystem ID namespaces. A simple approach:
   the high 16 bits = subsystem enum; the low 16 bits = per-subsystem counter ordinal.
   No central registry needed; each subsystem declares its own enum starting from 0.

4. **Integration with Tracy / Optick**: if the project ever targets a profiler-friendly
   workflow, the ring buffer + serializer thread design is compatible with Tracy's
   `TracyPlot` macro (push a named value per frame). This is worth noting as a future
   upgrade path rather than a current requirement.
