# Debugging Implementation Area

This folder is reserved for modern debugging utility implementation units.

It is build-wired through `src/wscript` as the first tested modern utility
slice. Code here still remains private to modernized internals and must stay
behind legacy-compatible adapters.

Expected future files:

- snapshot capture helpers
- human formatters
- JSON writers
- trace queue or ring buffer implementation
- console-backed and test-buffer-backed debug sinks

Current files:

- `json_writer.cpp`: streaming JSON object/array writer and string escaping
- `logging.cpp`: logging level names, gate behavior, and logger dispatch
- `snapshot_writer.cpp`: shared snapshot header human and JSON output helpers
- `trace.cpp`: trace level names and runtime trace gate behavior

Do not place public SDK or ABI surfaces here. Public engine compatibility
remains in the existing legacy modules until a later migration decision says
otherwise.

See also:

- `Documentation/codex/modern/debugging/architecture.md`
- `Documentation/codex/modern/debugging/api-inventory.md`
- `Documentation/codex/modern/thread-safe-debugging-utilities.md`
