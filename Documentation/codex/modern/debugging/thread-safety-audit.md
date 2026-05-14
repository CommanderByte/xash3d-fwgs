# Debugging Thread-Safety Audit

## Purpose

This document records the current thread-safety guarantees for the modern
debugging utility layer.

The goal is not to claim the whole future debugging stack is fully concurrent.
The goal is to make the current utility primitives data-race resistant where
they are likely to be shared by logging, tracing, and diagnostic commands.

## Current Guarantees

| Component | Current Thread-Safety |
| --- | --- |
| `TraceGate` | Thread-safe concurrent reads/writes through atomics. |
| `LogGate` | Thread-safe concurrent reads/writes through atomics. |
| `Logger` sink pointer | Atomic pointer load/store. Sink object lifetime remains caller-owned. |
| `BoundedTraceQueue` | Thread-safe `push`, `pop`, `clear`, `size`, `full`, and `stats` through a small spinlock. |
| `TraceQueueStats` | Read under queue lock through `BoundedTraceQueue::stats()`. |
| `FixedBufferDebugSink` | Not thread-safe; caller-owned scratch output sink. |
| `IDebugSink` implementations | Interface only; thread-safety depends on implementation. |
| `ILogSink` implementations | Interface only; thread-safety depends on implementation. |
| Snapshot writer functions | Reentrant when the supplied sink is safe for the calling pattern. |
| `json::JsonWriter` | Reentrant per writer instance when the supplied sink is safe for the calling pattern. Do not share one writer instance across threads. |

## Important Boundaries

`Logger` uses an atomic sink pointer so updating the pointer does not race with
readers at the pointer level. It does not own the sink object and cannot prove
the pointed-to sink remains alive while another thread writes through it.

Current rule:

- sink lifetime must outlive all concurrent `Logger::write` calls that may use
  it
- sink implementation must provide its own synchronization if multiple threads
  write to it
- replacing sinks concurrently is allowed only when the old sink remains alive
  until concurrent writers have drained

Preferred engine rule:

- create the primary debug-build log sink during engine startup
- keep it alive until debug/log producers have stopped during engine shutdown
- treat temporary replacement sinks as scoped test/tooling behavior, not the
  normal runtime path

`BoundedTraceQueue` is synchronized, but it is not wait-free and does not start
worker threads. It is a fixed-capacity primitive that can be used by a future
async trace drain.

## Tests

Current multithreading tests live in `tests/debugging/debugging.cpp`.

Coverage:

- concurrent `BoundedTraceQueue::push` from multiple threads
- concurrent `TraceGate` mutation and reads
- concurrent `LogGate` mutation and reads

The tests verify no event accounting is lost under concurrent queue pushes:

```text
stats.enqueued + stats.dropped == attempted pushes
```

## Remaining Work

- Add sink implementation tests once console/file/tooling sinks exist.
- Add producer/drain tests when a real async trace drain is introduced.
- Consider replacing the spinlock with a platform-neutral mutex or an external
  queue only if profiling shows contention or blocking risk.
- Document owner-thread rules for filesystem snapshot capture before filesystem
  debug adapters walk legacy state.
