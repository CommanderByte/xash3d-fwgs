# Debugging API Inventory

## Purpose

This document lists the proposed namespaces, classes, structs, and source homes
for the modern debugging utility layer.

It is a design inventory, not a build contract yet. Names can still change
before implementation, but future code should preserve the ownership,
threading, and ABI boundaries described here.

## Namespace Plan

| Namespace | Ownership | Intended Use |
| --- | --- | --- |
| `xash::debugging` | Shared modern debugging layer | Snapshot headers, sinks, formatters, JSON writers, trace gates, trace queues. |
| `xash::debugging::json` | Shared JSON helpers | Low-level JSON escaping/writer helpers if they stay independent of a third-party JSON library. |
| `xash::debugging::test` | Test-only helpers | Buffer sinks, fixed clocks, deterministic sequence generators. |
| `xash::filesystem::debugging` | Filesystem pilot adapter | Filesystem mount, lookup, registry, and find-all snapshot records. |

Rules:

- Do not expose these namespaces through public SDK or C ABI headers.
- Keep shared debugging types subsystem-neutral.
- Put filesystem-specific records under `xash::filesystem::debugging` until
  they prove reusable.
- Avoid a namespace per implementation file. Add namespaces only when they
  clarify ownership.

Code snippets below use namespace names as design notation. The implementation
should spell namespaces in whatever form matches the final minimum C++ standard;
do not treat C++17 nested namespace syntax as decided here.

## Header And Source Plan

| Future Path | Contents |
| --- | --- |
| `src/include/debugging/debug_types.hpp` | Shared enums, small IDs, schema header records. |
| `src/include/debugging/debug_sink.hpp` | `IDebugSink`, output format declarations, sink status. |
| `src/include/debugging/buffer_sink.hpp` | Fixed caller-buffer sink for no-allocation formatting and tests. |
| `src/include/debugging/json_writer.hpp` | Streaming JSON writer and string escaping helpers. |
| `src/include/debugging/logging.hpp` | Log levels, categories, records, sinks, gate, and logger facade. |
| `src/include/debugging/snapshot_writer.hpp` | Shared snapshot header writer declarations. |
| `src/include/debugging/trace.hpp` | Trace levels, categories, events, gates, counters, and bounded queue. |
| `src/debugging/debug_sink.cpp` | Common sink helpers and possible console sink adapter. |
| `src/debugging/json_writer.cpp` | Shared streaming JSON writer implementation. |
| `src/debugging/snapshot_writer.cpp` | Shared human/JSON writer helpers. |
| `src/debugging/trace.cpp` | Trace gate, counters, and queue implementation. |
| `src/filesystem/*debug*` | Filesystem-specific snapshot records and capture adapters. |
| `tests/debugging/*` | Reusable debugging utility tests. |
| `tests/filesystem/*debug*` | Filesystem integration tests for debug snapshots and output. |

Private modern C++ debugging headers use `.hpp` so mixed C/C++ code can quickly
distinguish C++-only contracts from legacy C-compatible `.h` headers.

Initial implementation note: `debug_types.hpp`, `debug_sink.hpp`,
`buffer_sink.hpp`, `json_writer.hpp`, `logging.hpp`, `snapshot_writer.hpp`,
`trace.hpp`, `src/debugging/json_writer.cpp`, `src/debugging/logging.cpp`,
`src/debugging/snapshot_writer.cpp`, and `src/debugging/trace.cpp` now exist
as the first shared utility slice. Filesystem-specific capture types are still
pending.

## Shared Core Types

### `SnapshotSchema`

Purpose: identify machine-readable snapshot output.

Proposed shape:

```cpp
namespace xash::debugging
{
struct SnapshotSchema
{
    const char *name;
    unsigned version;
};
}
```

Notes:

- `name` examples: `xash3d.fs.mounts`, `xash3d.fs.lookup`.
- `version` increments when machine-readable fields change incompatibly.
- Human output does not define schema compatibility.

### `SnapshotHeader`

Purpose: common metadata for every captured snapshot.

Proposed shape:

```cpp
namespace xash::debugging
{
struct SnapshotHeader
{
    SnapshotSchema schema;
    uint64_t sequence;
    uint64_t timestampUsec;
};
}
```

Ownership:

- `schema.name` must point to static storage.
- timestamp source is injected or centralized later for deterministic tests.

### `DebugOutputFormat`

Purpose: distinguish human-readable and machine-readable output.

Proposed values:

```cpp
namespace xash::debugging
{
enum class DebugOutputFormat
{
    Human,
    Json,
};
}
```

### `DebugStatus`

Purpose: local status for debug utility failures before a shared engine
`Status`/`Result<T>` utility exists.

Proposed shape:

```cpp
namespace xash::debugging
{
enum class DebugStatusCode
{
    Ok,
    AllocationFailed,
    InvalidArgument,
    BufferTooSmall,
    SinkUnavailable,
    UnsupportedFormat,
};

struct DebugStatus
{
    DebugStatusCode code;
    const char *message;

    bool ok() const;
};
}
```

Decision: `DebugStatus` stays local to `xash::debugging`. Release-oriented
core code should not need to depend on debugging utilities, trace gates, JSON
helpers, or debug sinks. If a future shared `xash::core::Status` exists,
subsystems can adapt between the two at explicit boundaries instead of making
one layer depend on the other.

## Sink Types

### `IDebugSink`

Purpose: abstract final output target.

Proposed shape:

```cpp
namespace xash::debugging
{
class IDebugSink
{
public:
    virtual ~IDebugSink() = default;
    virtual DebugStatus write(DebugOutputFormat format, const char *text) = 0;
};
}
```

Rules:

- sink implementations may be platform-specific
- formatters should know only about `IDebugSink`
- sinks should not mutate subsystem runtime state

### `ConsoleDebugSink`

Purpose: write debug output to the engine console.

Initial location: `src/debugging/debug_sink.cpp`, with adapter hooks near the
engine console owner if direct linking requires it.

Notes:

- may need a C adapter because the existing console surface is C-style
- should be synchronous at first
- should not own formatting policy

### `BufferDebugSink`

Purpose: collect output into caller-provided storage.

Implemented as `FixedBufferDebugSink` in `src/include/debugging/buffer_sink.hpp`.

Notes:

- deterministic and allocation-free
- useful for human formatter and JSON writer tests
- useful for runtime commands that want caller-owned scratch buffers
- exposes captured text without console dependencies

## Writer Types

### `HumanSnapshotWriter`

Purpose: common helper for human-readable formatting.

Proposed responsibilities:

- indentation
- stable table-like sections
- newline handling
- output through `IDebugSink` or an intermediate buffer

Do not make this class query runtime state. It consumes snapshot records only.

### `json::JsonWriter`

Purpose: emit stable JSON from snapshot records.

Implemented responsibilities:

- object/array delimiters
- object field names and comma state
- string escaping
- unsigned and signed integer values
- boolean and null values
- buffer overflow reporting

Implementation choice:

- use a small streaming writer over `IDebugSink`
- avoid a DOM or heap allocation for the first debug-command serializers
- keep the class boundary clean enough to replace with RapidJSON, yyjson, or
  another backend later

RapidJSON is deferred because the current need is JSON generation for debug
snapshots, not parsing or arbitrary DOM manipulation. If filesystem snapshots
become deeply nested or schema-heavy, the writer facade is where that decision
should be revisited.

### `SnapshotFormatter<TSnapshot>`

Purpose: optional template or overload pattern for snapshot-specific formatting.

Possible shape:

```cpp
namespace xash::debugging
{
DebugStatus writeHuman(IDebugSink &sink, const FilesystemMountSnapshot &snapshot);
DebugStatus writeJson(IDebugSink &sink, const FilesystemMountSnapshot &snapshot);
}
```

Recommendation: prefer free functions or small stateless formatter objects for
the first implementation. Avoid inheritance-heavy formatter trees until there
are several consumers.

## Trace Types

### `TraceLevel`

Purpose: define trace verbosity.

Proposed values:

```cpp
namespace xash::debugging
{
enum class TraceLevel
{
    Error,
    Warn,
    Info,
    Debug,
    Verbose,
    Trace,
};
}
```

### `TraceCategory`

Purpose: identify trace streams without string parsing in hot paths.

Proposed shape:

```cpp
namespace xash::debugging
{
struct TraceCategory
{
    const char *name;
    TraceLevel compiledMaximumLevel;
};
}
```

Subsystems can define static categories, such as:

```cpp
namespace xash::filesystem::debugging
{
extern const xash::debugging::TraceCategory LookupTrace;
extern const xash::debugging::TraceCategory MountTrace;
}
```

### `TraceEvent`

Purpose: compact event record for trace output.

Proposed shape:

```cpp
namespace xash::debugging
{
struct TraceEvent
{
    uint64_t sequence;
    uint64_t timestampUsec;
    TraceLevel level;
    const TraceCategory *category;
    const char *message;
};
}
```

First implementation note: `message` can point to stable or queue-owned storage.
Do not enqueue pointers to temporary formatted strings.

### `TraceGate`

Purpose: decide whether trace work should happen.

Proposed shape:

```cpp
namespace xash::debugging
{
class TraceGate
{
public:
    bool enabled(const TraceCategory &category, TraceLevel level) const;
    void setEnabled(bool enabled);
    void setRuntimeMaximumLevel(TraceLevel level);
};
}
```

Rules:

- check before formatting
- runtime controls can be wired later through cvars or developer settings
- compile-time gating should remove expensive trace paths in release builds

### `BoundedTraceQueue`

Purpose: fixed-capacity event buffer with explicit overflow behavior.

Proposed shape:

```cpp
namespace xash::debugging
{
enum class TraceOverflowPolicy
{
    DropNewest,
    DropOldest,
};

struct TraceQueueStats
{
    uint64_t enqueued;
    uint64_t dropped;
};

class BoundedTraceQueue
{
public:
    bool push(const TraceEvent &event);
    bool pop(TraceEvent &event);
    TraceQueueStats stats() const;
};
}
```

Implementation note: do not add this until the synchronous path has tests or a
trace producer proves the need.

Current implementation note: `BoundedTraceQueue` exists, but it does not create
threads or imply async behavior by itself. It uses caller-provided storage and
supports `DropNewest` and `DropOldest` overflow policies.

Thread-safety note: queue operations are protected by a small internal
spinlock. See [thread-safety-audit.md](thread-safety-audit.md) for current
guarantees and caveats.

## Logging Types

Logging is the lower-volume event stream for diagnostics that should remain
available outside trace-only instrumentation.

### `LogLevel`

Purpose: define diagnostic severity and verbosity.

Implemented values:

```cpp
enum class LogLevel
{
    Error,
    Warn,
    Info,
    Debug,
    Trace,
};
```

### `LogCategory`

Purpose: identify log streams and their compiled maximum verbosity.

Implemented fields:

- `name`
- `compiledMaximumLevel`

### `LogRecord`

Purpose: pass one preformatted log event to a sink.

Implemented fields:

- `sequence`
- `timestampUsec`
- `level`
- `category`
- `message`

### `ILogSink`

Purpose: abstract a logging backend.

Expected future sinks:

- engine console log sink
- test recording sink
- optional file/tooling sink
- optional `spdlog` sink backend

### `LogGate`

Purpose: runtime log enablement and level checks.

Unlike `TraceGate`, `LogGate` starts enabled with `Info` as the default maximum
level. Error, warning, and info diagnostics should not require opt-in tracing.

### `Logger`

Purpose: combine a sink with a `LogGate` and dispatch preformatted records.

The first implementation does not format messages. Callers pass stable text.
Formatting policy remains deferred until the project decides whether to use
`{fmt}` or another backend.

The logger owns an atomic sink pointer, not the sink object. In normal engine
debug builds, the intended lifetime model is to create the primary log sink
during startup and keep it alive until debug producers have stopped during
shutdown. Temporary sink replacement is acceptable for tests or tooling only
when the previous sink remains alive long enough for concurrent writers to
drain.

## Filesystem Pilot Types

Filesystem-specific debug records should start in `xash::filesystem::debugging`
and stay out of the shared debugging namespace.

### `FilesystemMountRecord`

Purpose: describe one mounted search path.

Proposed shape:

```cpp
namespace xash::filesystem::debugging
{
struct FilesystemMountRecord
{
    int order;
    const char *type;
    const char *source;
    uint32_t flags;
    bool writable;
    bool archive;
    const char *mountReason;
};
}
```

### `FilesystemMountSnapshot`

Purpose: immutable view of mounted search paths.

Proposed shape:

```cpp
namespace xash::filesystem::debugging
{
struct FilesystemMountSnapshot
{
    xash::debugging::SnapshotHeader header;
    const FilesystemMountRecord *records;
    size_t recordCount;
};
}
```

Ownership question: the first implementation must decide whether snapshots own
a vector-like buffer, use an arena, or write into caller-provided storage.
Caller-provided storage is attractive for early tests and low allocation risk.

### `FilesystemLookupStep`

Purpose: record one search path checked during lookup.

Fields to consider:

- search path order
- backend type
- source
- requested path
- fixed path if case correction changed it
- result: rejected, not found, found
- rejection reason if applicable

### `FilesystemLookupSnapshot`

Purpose: explain `fs_why <path>`.

Fields to consider:

- requested path
- normalized path
- direct-path policy state
- ordered lookup steps
- winning path/backend
- final status

### `FilesystemRegistrySnapshot`

Purpose: describe registered archive/backend descriptors.

Fields to consider:

- extension
- backend name
- archive type
- real archive versus directory-like archive
- auto-mount WAD behavior
- supported flags

### `FilesystemDebugCapture`

Purpose: capture filesystem snapshots from current runtime state.

Proposed shape:

```cpp
namespace xash::filesystem::debugging
{
class FilesystemDebugCapture
{
public:
    xash::debugging::DebugStatus captureMounts(
        FilesystemMountSnapshot &out) const;
};
}
```

First implementation note: this class can be a thin adapter over existing C
state. It should not force the filesystem rewrite before snapshots are useful.

## Ownership Patterns

Preferred early options:

| Pattern | Where To Use | Why |
| --- | --- | --- |
| Caller-provided buffer | First snapshot tests and mount snapshots | Avoids allocator policy decisions. |
| Snapshot-owned arena | Larger lookup snapshots later | Keeps string and record lifetime simple. |
| Immutable shared snapshot | Future concurrent read paths | Allows lock-free read-side diagnostics after publication. |

Avoid:

- raw pointers to mutable linked-list nodes in published snapshots
- formatting directly into the console while holding filesystem state ownership
- dependency-specific containers in public-private headers before dependency
  decisions are made

## Initial Creation Order

1. [x] `debug_types.hpp`: `SnapshotSchema`, `SnapshotHeader`, `DebugStatus`,
   `DebugOutputFormat`.
2. [x] `debug_sink.hpp`: `IDebugSink`.
3. [x] `snapshot_writer.hpp`: shared human and JSON snapshot header writer
   declarations.
4. [x] `buffer_sink.hpp`: `FixedBufferDebugSink`.
5. [x] `json_writer.hpp`: streaming JSON writer object.
6. [x] Filesystem `FilesystemMountRecord` and `FilesystemMountSnapshot`.
7. [ ] Filesystem mount capture adapter using caller-provided storage.
8. [x] Human snapshot header writer.
9. [x] JSON snapshot header writer.
10. [x] Human mount snapshot writer.
11. [x] JSON mount snapshot writer.
12. [x] Trace category and event declarations.
13. [x] Runtime `TraceGate`.
14. [x] Logging facade declarations and dispatch.
15. [x] Compile-time trace macro policy note.
16. [x] Bounded trace queue with caller-provided storage.

This order keeps the first code slice small enough to review while still
establishing the reusable debugging vocabulary.
