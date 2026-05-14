# Launcher JSON Policy

## Current Position

The launcher currently has one small JSON reader for optional startup defaults
and the debugging utilities have a JSON writer for structured diagnostics.
Those are different needs: the debugging writer does not parse input, while the
launcher reader parses a flat object with a handful of string and boolean
fields.

For now, the local launcher reader is acceptable because:

- the accepted schema is intentionally flat;
- config files are optional and compiled defaults remain authoritative;
- parser failure must be recoverable;
- adding a dependency now would be more architecture than the feature needs.

This should not become a pattern. If another runtime JSON reader appears, or if
launcher config grows nested objects, arrays, schema validation, comments, or
round-trip editing, switch to a shared library instead of expanding local
parsers.

## Candidate Libraries

| Library | Fit | Tradeoff |
| --- | --- | --- |
| [yyjson](https://github.com/ibireme/yyjson) | Best first candidate for small, fast runtime parsing in mixed C/C++ code. | C API; we would wrap it behind `src/include/utilities/json.hpp` or similar. |
| [RapidJSON](https://github.com/Tencent/rapidjson) | Good if we want a mature C++ DOM/SAX parser and writer with explicit allocator control. | Older style API and more template/header surface. |
| [simdjson](https://github.com/simdjson/simdjson) | Excellent for very large JSON files or high-throughput ingestion. | Overkill for tiny launcher config and typically wants newer C++ assumptions. |
| [nlohmann/json](https://github.com/nlohmann/json) | Very ergonomic for tools, tests, and non-hot paths. | Heavy compile-time footprint and not the best fit for engine/runtime code. |
| [Boost.JSON](https://www.boost.org/library/latest/json/) | Solid if the project already chooses Boost as a broader dependency. | Pulling Boost solely for launcher config is too much weight. |

## Recommendation

Do not add a JSON dependency for the current launcher-only reader.

When a second runtime parser is needed, prefer `yyjson` first because it is
small, fast, C-friendly, and easy to keep behind a narrow wrapper. If the future
need is mostly C++ object ergonomics rather than engine runtime performance,
re-evaluate RapidJSON or nlohmann/json at that point.

## Wrapper Rules

If a library is introduced:

- hide it behind a tiny project-owned API;
- do not expose third-party JSON types across subsystem boundaries;
- keep parse failures explicit and recoverable;
- keep debug JSON writing independent unless the new library clearly improves
  it;
- add tests for missing file, invalid JSON, unknown keys, type mismatch, and
  default fallback behavior.
