# Formatting Policy

## Purpose

This document records how human-readable formatting should evolve for the
modern debugging utilities.

Machine-readable snapshots should remain boring and stable. Human-readable
logs and command output can use richer formatting, but formatting choices should
not leak third-party types through public or legacy ABI boundaries.

## Current Position

The current code accepts preformatted logging messages and uses tiny internal
writers for snapshot headers. This is intentional for the first slice:

- no new dependency yet
- no public ABI exposure
- easy unit tests
- predictable output for machine-readable diagnostics

This should not grow into a full custom formatting library.

## `{fmt}` Recommendation

`{fmt}` is the preferred candidate once we need type-safe formatting.

Reasons:

- fast and widely used
- mature C++ formatting API
- familiar backend for `spdlog`
- suitable for human-readable log messages and console/debug command output

Constraints:

- wrap it behind engine-owned helpers
- do not expose `{fmt}` types in public SDK or legacy ABI headers
- do not use formatting APIs in hot trace paths unless the trace gate has
  already passed
- keep snapshot JSON serialization separate from pretty formatting

## Where Formatting Belongs

Good candidates:

- log message formatting
- human debug command output
- developer-facing error text
- optional console/file sinks

Poor candidates:

- stable JSON snapshot schemas
- C ABI callback boundaries
- disabled trace paths
- low-level status objects that should stay allocation-light

## Next Decision Point

Add `{fmt}` only when one of these becomes true:

- filesystem debug commands need enough human formatting that manual string
  assembly becomes noisy
- logging producers need typed formatting at call sites
- an `spdlog` backend is being evaluated

Until then, keep messages preformatted and keep the dependency list smaller.
