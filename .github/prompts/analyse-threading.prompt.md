---
name: "Analyse threading safety"
description: "Audit a subsystem (legacy or xash3dpp) for data races, unsafe statics, and missing synchronisation, then append a Threading section to its boundary spec."
argument-hint: "subsystem name matching an existing boundary spec (e.g. public-utilities, filesystem, sound)"
agent: agent
tools: [read, search, edit]
---

# Threading Safety Audit: $ARGUMENTS

You are auditing the **$ARGUMENTS** subsystem for thread-safety hazards. You will
read both the legacy source and (if it exists) the new `xash3dpp/` skeleton, then
append a `## Threading` section to the boundary spec doc.

Do **not** add locks or refactor code — analysis and documentation only.

## Step 1 — Locate all state

Search the relevant source files for every piece of shared mutable state:

- `static` local variables (including function-local statics)
- File-scope globals (`static` or otherwise)
- Heap objects reachable from module-level pointers
- `extern` variables declared in headers

For each item record:
- Name and type
- Access pattern: read-only after init / per-call (caller-owned) / mutable shared

## Step 2 — Classify each hazard

For every mutable-shared item from Step 1, classify it:

| Class | Description |
|-------|-------------|
| **Safe-RO** | Written once at startup, read-only from that point on. Safe for concurrent reads. |
| **Safe-TLS** | Each caller passes its own state object; no sharing. |
| **Race-static-buf** | Function returns a pointer into a static buffer. Concurrent calls corrupt each other's result. |
| **Race-lazy-init** | Static local or double-checked locking that is not atomic — data race on first use. |
| **Race-shared** | Mutable global / singleton mutated during normal operation — needs a lock or redesign. |
| **Signal-unsafe** | Called from signal/interrupt context but uses non-async-signal-safe operations (malloc, stdio, locks). |

## Step 3 — Check ownership model

Determine which threads are expected to call this subsystem:

- Is it main-thread-only? If so, is that enforced (assert) or merely assumed?
- Does the server thread, sound thread, or filesystem I/O thread call into it?
- Is there a documented "init phase" after which the module becomes read-only?

Look for any existing locks (`SDL_mutex`, `pthread_mutex`, critical sections) and
note whether they cover all access paths.

## Step 4 — Check lazy-init patterns

Find any one-time initialisation:

- C++17 magic-statics (`static Foo f = ...` inside a function) — these are
  thread-safe in C++11 and later **if** the runtime support is present (it is
  absent when `-fno-threadsafe-statics` is passed; check CMakeLists).
- Hand-rolled double-checked locking — safe only with `std::atomic` and the
  correct memory order.
- `com_initialized` / `bInit` style boolean guards — not safe without a fence.

## Step 5 — Check signal-handler safety

Flag any function that:
- Is called from an engine signal handler or interrupt service routine, AND
- Uses `malloc`/`free`, `printf`, `fwrite`, mutexes, or C++ static initialisers.

These must be replaced with pre-allocated buffers and async-signal-safe I/O.

## Step 6 — Write the threading analysis

Read `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` for context on owned
state and the subsystem's responsibility, then create a new file:

`xash3dpp/docs/threading-analysis/$ARGUMENTS-threading.md`

```markdown
# <Subsystem> Threading Analysis

> Boundary spec: `docs/boundaries/<subsystem>-boundary.md`

## Ownership model
<!-- Which thread(s) are the legitimate callers?  Is that enforced (assert / docs only)? -->

## Safe items
<!-- Bullet list: items classified Safe-RO or Safe-TLS, with one-line justification. -->

## Hazards
<!-- Table of Race-* and Signal-unsafe items found in Steps 1–5. -->
| Symbol | File | Class | Notes |
|--------|------|-------|-------|

## Required caller contracts
<!-- What synchronisation must callers provide that this module does not? -->

## Recommendations
<!-- Ordered list: quick fixes first (e.g. static buf → thread_local,
     add assert_main_thread), then deeper redesigns. -->
```

Do not add any synchronisation primitives to source files. The analysis document is the deliverable.
