# Trace Gating Policy

## Purpose

This document defines the policy for trace gating before trace macros or async
trace producers are added.

Trace code can easily become expensive in hot paths. The debugging layer should
therefore make disabled tracing cheap, keep release-build behavior explicit,
and avoid forcing unrelated release code to depend on debugging utilities.

## Layers

Trace gating has three layers:

1. Compile-time availability.
2. Runtime category and level gating.
3. Optional async buffering after an event has already passed the first two
   gates.

The current implementation covers the second layer with `TraceGate`. The first
layer should be added before any high-volume trace macros are introduced. The
third layer remains deferred until there is a real producer.

## Compile-Time Policy

Future trace macros should follow these rules:

- Disabled trace builds must avoid evaluating expensive message arguments.
- Release builds should compile out verbose and trace-level work unless a build
  option explicitly keeps it.
- Error and warning logs are not the same as trace events; do not hide required
  diagnostics behind trace-only macros.
- Compile-time gates should be controlled by a small number of project defines,
  not scattered subsystem-specific `#ifdef` blocks.
- Macros should call into typed functions after gating, so most behavior stays
  testable without preprocessor gymnastics.

Possible future define names:

```c
XASH_DEBUGGING_TRACE_ENABLED
XASH_DEBUGGING_TRACE_MAX_LEVEL
```

These names are not implemented yet. They are placeholders for the policy
shape.

## Runtime Policy

`TraceGate` currently owns runtime enablement and maximum level checks.

Rules:

- runtime trace starts disabled
- callers check `TraceGate::enabled(category, level)` before formatting
- empty or null category names are treated as disabled
- a trace event must pass both runtime maximum level and category compiled
  maximum level

This gives each subsystem a static category cap while still allowing runtime
controls to narrow or widen enabled diagnostics.

## Async Policy

Async queues are intentionally deferred.

Before enabling async trace buffering, the implementation must define:

- capacity
- overflow behavior
- dropped-event counters
- shutdown flush/discard behavior
- thread ownership for draining

Until then, trace support should stay synchronous and cheap when disabled.

## First Producer Checklist

Before adding the first real trace producer:

- add compile-time macro gates
- add tests proving disabled trace calls do not evaluate expensive expressions
- choose the runtime control source, such as cvars or developer settings
- confirm category names and levels in `debugging_todo.md`
- keep async queue work deferred unless synchronous trace has measured cost
