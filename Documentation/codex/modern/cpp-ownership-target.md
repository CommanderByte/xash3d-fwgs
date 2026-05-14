# C++ Ownership Target

This note records a project-wide guardrail: wrapping old C functions in a C++
namespace is a migration tool, not the final modernization target.

The iterative approach is still the right way to keep the engine runnable, but
each migrated area should eventually move through four stages.

## Stages

### 1. Compatibility Facade

The first stage preserves the public or legacy-facing surface:

- C ABI functions keep their names and calling conventions;
- public SDK, game DLL, client DLL, renderer, filesystem, and platform exports
  do not expose private C++ types;
- adapters translate legacy structs, globals, flags, and buffers into plain
  snapshots or simple values.

This is where much of the current work lives. It is useful because it creates
tests and prevents compatibility drift, but it is not the destination.

### 2. Behavior Ownership

The next stage names the concept that actually owns the behavior:

- `GroupFilterPolicy`, not "a wrapper around three `FBitSet` calls";
- `MapValidationResult`, not "map flags in a namespace";
- `ClientCapabilities`, not raw `FCL_*` checks copied into C++;
- `EventPlaybackPlan`, not a C++ clone of `SV_PlaybackEventFull()`;
- `ResourceCatalog`, `SearchPath`, `CommandRegistry`, or `CvarSnapshot` where
  those concepts match real engine responsibilities.

The modern code should use explicit types, result objects, ownership boundaries,
and tests that describe intent. Compatibility quirks should be documented as
quirks, not hidden behind prettier function names.

### 3. Module Consolidation

Once several related helpers exist, regroup them by responsibility instead of
leaving one helper per old C function.

Examples:

- event filtering, group filtering, and client recipient predicates can become
  part of an event/visibility decision module;
- save-format fixtures, map validation, and changelevel intent can converge
  toward a save/changelevel boundary;
- query payloads, NetAPI responses, and connectionless classification can
  become a coherent server query/connection module;
- filesystem backend bridges can eventually collapse into real backend objects
  plus a single legacy export layer.

This phase may move files, rename classes, or split directories, but only after
tests protect behavior.

### 4. Idiomatic C++ Internals

The long-term internal code should use C++ where it genuinely helps:

- RAII for owned resources, library handles, buffers, and temporary allocations;
- typed enums and small value objects instead of open integer/bitmask plumbing;
- immutable snapshots for cross-boundary reads and diagnostics;
- narrow interfaces with composition instead of large inheritance hierarchies;
- containers where ownership and lifetime are clearer than manual arrays;
- explicit status/result returns at legacy boundaries;
- exceptions only inside carefully controlled private code, never across C ABI
  or plugin/DLL boundaries unless a later decision allows that.

The goal is not "C++ for its own sake." The goal is less painful extension,
clearer ownership, better testability, and fewer hidden global dependencies.

## File Layout Target

Modern files should ultimately be grouped by concept and subsystem, not by the
original C file they came from.

Good signs:

- headers describe concepts rather than old source file names;
- adapters are visibly separated from pure implementation;
- shared utilities live under `src/utilities`, not in a server-specific folder;
- platform code lives under platform folders or platform-selected build units;
- compatibility layers are thin and boring.

Bad signs:

- a modern file is only `SV_OldFunctionName()` with namespace syntax;
- one adapter keeps growing unrelated responsibilities;
- modern code includes legacy mega-headers just to access globals directly;
- tests only verify byte-for-byte forwarding and never describe the concept;
- file count rises but ownership does not become clearer.

## Checkpoint Rule

After a lane accumulates several helpers, schedule a consolidation checkpoint.
At that checkpoint, ask:

1. Which helpers are still temporary facades?
2. Which helpers now form a real domain concept?
3. Which files should be renamed, regrouped, or merged?
4. Which legacy adapters can shrink?
5. Which tests should move from "old behavior mirror" to "modern concept
   contract" while retaining compatibility coverage?

This keeps the migration honest. The scaffolding is allowed, but it should not
quietly become the architecture.
