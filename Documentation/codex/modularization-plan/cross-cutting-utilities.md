# Cross-Cutting Modernization Utilities

## Purpose

This document identifies reusable utilities that should exist outside any one
subsystem during the rewrite. Filesystem work is the pilot, but these concepts
are useful for renderer, platform, resource loading, commands/cvars, UI, and
future tooling too.

The goal is to avoid turning each subsystem into its own little kingdom with
different logging, serialization, error, registry, and threading conventions.

## Guiding Rules

- Keep C ABI boundaries C-compatible.
- Keep public SDK headers free of STL, exceptions, and private C++ utility
  types.
- Prefer small facades over direct dependency usage in subsystem code.
- Prefer snapshot records for diagnostics and serialization.
- Prefer explicit ownership and explicit status returns over implicit global
  behavior.
- Prepare for concurrency by isolating mutable state, not by adding locks
  everywhere.

## Utility Areas

| Area | Shared Utility | First Consumers |
| --- | --- | --- |
| Errors | `Status`, `Result<T>`, error category/severity enums | Filesystem, platform, archive loaders |
| Logging | `LogSink`, category/level logging facade, optional async backend | Filesystem debug tools, engine diagnostics |
| Formatting | Type-safe formatting wrapper | Logging, console commands, JSON errors |
| Serialization | Snapshot structs plus JSON writer | Debug commands, tests, automation |
| Registry | Generic ordered registry | Archives, commands/cvars later, asset loaders |
| Threading | Mutex/atomic/queue/task abstractions | Logging, async loading, debug snapshots |
| Time/Profiling | Lightweight timers and scoped trace events | Debug utilities, performance work |

## Recommended External Library Shortlist

The repo currently vendors domain libraries like miniz, libbacktrace, SDL, and
audio/render dependencies, but it does not vendor a general formatting,
logging, JSON, or concurrency utility stack.

Candidate libraries to evaluate before vendoring:

| Need | Candidate | Why Consider It | Caution |
| --- | --- | --- | --- |
| Formatting | `{fmt}` (`fmtlib/fmt`) | Fast, type-safe formatting; common backend for logging libraries. | Adds a new C++ dependency; wrap it behind our facade. |
| Logging | `spdlog` | Fast logging library with sync and async modes; uses fmt-style formatting. | Must route to engine console and respect existing developer/report controls. |
| JSON writing/parsing | `RapidJSON` | C++11-era fast SAX/DOM JSON parser/generator; good fit for generated debug JSON. | API is lower-level than nlohmann; wrap JSON output builders. |
| JSON parsing | `simdjson` | Very high-speed JSON parser if future tooling ingests large JSON. | Overkill for small debug output and not primarily a serializer. |
| JSON convenience | `nlohmann/json` | Extremely ergonomic and supports binary encodings too. | Slower/heavier; better for tools than hot engine paths. |
| C++20 serialization | `Glaze` | Very fast reflection-style JSON serialization. | Requires a modern C++ baseline; likely premature until standard policy is final. |
| Queues | `moodycamel::ConcurrentQueue` | C++11 MPMC queue often used for high-throughput async systems. | Use only if we need a custom async queue; `spdlog` already has async logging. |
| Enum strings | `magic_enum` | Header-only enum-to-string reflection for debug snapshots. | Requires C++17; can be avoided with explicit tables if baseline is lower. |

Recommendation:

1. Start with our own small interfaces and no hard dependency in core headers.
2. If we add one dependency first, prefer `{fmt}` because it improves logging
   and diagnostics immediately.
3. For runtime logging, evaluate `spdlog` only behind `LogSink` so console and
   platform behavior remain ours.
4. For debug JSON, prefer a small JSON writer or RapidJSON first. Add
   simdjson only when we have high-volume parsing, not merely because it is
   fast.
5. Do not select Glaze or `magic_enum` until the project has a firm C++17/C++20
   baseline.

## Error And Exception Utility

The shared error model should be independent of C++ exceptions.

Proposed shape:

```cpp
enum class ErrorDomain
{
    Filesystem,
    Platform,
    Archive,
    Renderer,
    Network,
    Console,
};

enum class ErrorSeverity
{
    Recoverable,
    Fatal,
};

struct Status
{
    ErrorDomain domain;
    int code;
    ErrorSeverity severity;
    const char *message;
};
```

For returned values:

```cpp
template<class T>
class Result
{
public:
    bool ok() const;
    const T &value() const;
    const Status &status() const;
};
```

Implementation note: if C++23 is not available, use a local small `Result<T>`
or evaluate a compact `expected`-style header later. Do not expose `Result<T>`
through public C ABI or legacy SDK headers.

Exception policy:

- no exceptions across C ABI boundaries
- no exceptions through engine callback tables
- no exceptions required in cross-platform engine internals until build policy
  changes
- if a library throws internally, catch at the facade boundary and convert to
  `Status`

## Logging Utility

Logging should be a facade, not direct `spdlog` calls sprinkled through engine
code.

Suggested public-private interface:

```cpp
enum class LogLevel { Error, Warn, Info, Debug, Trace };

struct LogCategory
{
    const char *name;
};

class LogSink
{
public:
    virtual ~LogSink() = default;
    virtual void write(LogLevel level, LogCategory category,
        const char *message) = 0;
};
```

Subsystem code should use helper functions or macros that can compile out trace
work:

```cpp
XASH_LOG_WARN(fs::log::Path, "rejected path {}", path);
XASH_LOG_TRACE(fs::log::Lookup, "checked {} => {}", mount, result);
```

Release/performance policy:

- errors and warnings remain available
- debug logs are runtime gated
- trace logs are compile-time gated and runtime gated
- expensive formatting should not happen when a log level is disabled
- async logging should be optional and bounded

If `spdlog` is used, it should be one possible `LogSink`, not the engine-wide
API itself.

## Serialization Utility

Use debug snapshot records plus serializers.

Do this:

```cpp
struct SearchPathDebugRecord
{
    int order;
    const char *type;
    const char *source;
    uint32_t flags;
    bool writable;
};
```

Avoid this:

```cpp
class DirectoryBackend
{
    std::string toJson() const; // couples backend logic to JSON formatting
};
```

Preferred architecture:

```text
runtime object -> debug snapshot -> human formatter
                              \-> JSON writer
```

Benefits:

- tests can inspect records directly
- JSON does not infect core logic
- human output and JSON stay consistent
- snapshots can be copied safely for async/debug use

JSON policy:

- JSON is the first machine-readable format
- include schema names such as `xash3d.fs.debug.v1`
- keep field names stable once tests depend on them
- do not use BSON unless a real binary transport requirement appears

## Registry Utility

The generic registry should be usable beyond archives.

Responsibilities:

- stable ordered registration
- lookup by key
- duplicate-key policy
- enumeration for debug output
- optional build-time/static registration later

Non-responsibilities:

- subsystem-specific ordering policy
- mutation of subsystem runtime state
- path validation
- logging every lookup

Potential users:

- archive formats
- command/cvar descriptors
- platform service backends
- serializer formats
- asset/image loader descriptors

## Thread-Readiness Plan

Do not jump straight to broad multithreading. Prepare for it by making state
ownership and mutation clear.

### First Principles

- Reads should eventually be able to happen concurrently.
- Mount/unmount/game change operations are mutations and should be explicit
  write-side operations.
- Debug snapshots should not hold locks while formatting or writing JSON.
- Background work should communicate through bounded queues or immutable
  results.
- Cross-thread callbacks into legacy engine code should be avoided unless an
  owner thread is defined.

### Filesystem Implications

Near-term:

- keep current single-threaded behavior
- define which APIs are read operations and which mutate state
- make debug snapshot builders copy data while holding a short lock later
- avoid adding hidden global mutable caches without an ownership note

Future:

- store search paths in an immutable snapshot for read-side lookup
- update snapshots atomically after mount/unmount operations
- protect write path and direct-path policy with explicit state ownership
- avoid per-file global locks on hot read paths
- make archive backends either immutable after mount or internally synchronized

Possible future shape:

```cpp
class FilesystemState
{
public:
    SearchPathSnapshot snapshot() const;
    Status replaceMounts(SearchPathSnapshot next);

private:
    // start with a mutex; only optimize to atomics/shared ownership after
    // tests and profiling prove the need.
};
```

### Debug Utility Threading

Debug utilities should be designed as:

```text
capture snapshot quickly -> release lock -> format human/JSON output
```

Trace mode should use a bounded ring buffer or bounded async queue. Dropped
trace events are acceptable if the output says events were dropped.

### Logging Threading

Async logging is useful, but it must be bounded:

- bounded queue
- explicit overflow behavior
- flush on shutdown
- never block the main thread indefinitely during shutdown
- avoid logging from memory allocation failure paths if the logger allocates

If `spdlog` is adopted, its async mode can satisfy this, but only through the
engine `LogSink` facade.

## Suggested Implementation Order

1. Add `Status` and `Result<T>` in `src/include/xash/core/`.
2. Add log level/category declarations and a console-backed `LogSink`.
3. Add debug snapshot structs for filesystem mounts.
4. Add human formatter for mount snapshots.
5. Add JSON writer for snapshot records.
6. Add generic registry template.
7. Wire filesystem archive metadata through the registry.
8. Add optional external library dependency only when a facade needs it.
9. Add lock/snapshot abstractions only when the first shared mutable state is
   ready to move.

## Dependency Gate Checklist

Before adding any new third-party utility dependency:

- license is compatible with the project
- works with the chosen minimum C++ standard
- builds on Windows, Linux, Android, and other supported targets
- can be vendored or submoduled cleanly under `3rdparty`
- does not expose types across public C ABI boundaries
- has a facade in `src/include` or a private subsystem wrapper
- has a small test or sample build path
- has a clear reason not to use existing project utilities

