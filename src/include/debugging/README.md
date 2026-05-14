# Private Debugging Headers

This folder is reserved for private C++ headers used by the modern debugging
utility layer.

Headers here should describe stable internal contracts without leaking
third-party utility types or STL-heavy implementation details into public ABI
headers.

Expected future header groups:

- snapshot record declarations
- sink interfaces
- formatter and writer interfaces
- trace category and level declarations
- bounded queue or ring buffer wrappers
- test helpers for snapshot inspection

Initial rules:

- keep headers private to modernized internals
- prefer plain data records for snapshots
- keep formatting and serialization separate from runtime objects
- avoid exceptions across C adapter and engine callback boundaries
- make ownership and lifetime explicit

Current headers:

- `debug_types.hpp`: shared schema, header, status, and output format types
- `debug_sink.hpp`: `IDebugSink`
- `buffer_sink.hpp`: fixed caller-buffer sink for no-allocation output capture
- `json_writer.hpp`: streaming JSON object/array writer over `IDebugSink`
- `logging.hpp`: log levels, categories, records, sinks, gate, and logger facade
- `snapshot_writer.hpp`: shared snapshot header writer declarations
- `trace.hpp`: trace level, category, event, stats, gate, and bounded queue
  declarations

Use `.hpp` for private modern C++ headers in this folder. Keep `.h` for C and
legacy ABI-oriented headers elsewhere unless that local code already has a
different convention.

See also:

- `Documentation/codex/modern/debugging/architecture.md`
- `Documentation/codex/modern/debugging/api-inventory.md`
- `Documentation/codex/modern/thread-safe-debugging-utilities.md`
