# External Architecture Review — xash3dpp

> **Reviewer**: Claude (Anthropic), acting as external technical reviewer
> **Date**: 2026-05-16
> **Documents reviewed**:
> - `decisions-architecture.md`
> - `decisions-style.md`
> - `threading-model.md`
> - `debug-stats-design.md`
> - `cmd_cvar-boundary.md`
> - `filesystem-boundary.md`
> - `memory-boundary.md`
> - `platform-boundary.md`
> - `public-utilities-boundary.md`
>
> **Scope**: architecture, threading, subsystem boundaries, and open questions.
> Not a line-by-line code review — no source files were available.

______________________________________________________________________

## 1. Overall Assessment

The architecture is remarkably coherent for an incremental rewrite of a 20+-year
legacy codebase. The document set demonstrates several properties that are hard
to retrofit and easy to lose:

- **Boundary-first design**: every subsystem has a written boundary spec before
  implementation. This is the single most valuable discipline in the project and
  should be preserved unconditionally.
- **Grandfathered-with-intent**: the "bring into conformance when next touched"
  policy avoids big-bang consistency passes that kill momentum. Every exception
  is documented with a trigger for when it migrates.
- **ABI preservation without ABI contamination**: the frozen GoldSrc structs
  (`cvar_t`, `enginefuncs_t`, etc.) are acknowledged as immovable constraints,
  but internal code is cleanly separated from them via shims and adapter layers.
- **Threading as interface property**: designing `const T&` signatures now to
  enable parallelism later is the highest-leverage decision in the project. It
  costs nothing today and avoids a redesign later.

The remainder of this document is structured as: things to reconsider, things
to watch, specific feedback on boundary specs, and responses to open questions
where I have an opinion.

______________________________________________________________________

## 2. Architecture-Level Feedback

### 2.1 `shared_ptr<JobToken<T>>` — reconsider the allocation model

The `JobToken` design (§4 of `threading-model.md`) is clean. The
`shared_ptr` return from `submit_job` is the one texture mismatch with the rest
of the ownership model.

**Concern**: `shared_ptr` implies a control block allocation on every job
submission. The project has been disciplined about avoiding unnecessary heap
allocations everywhere else (`pool_ptr`, stack-buffer `logf`, `noexcept`
throughout). A shared-pointer per job is a small but real cost, and it couples
the job lifetime to reference-count semantics that may not match the actual
ownership pattern (main thread submits, main thread polls, main thread consumes
— this is effectively single-owner with a brief period of shared visibility).

**Alternatives to evaluate**:

- **Pool-allocated tokens with explicit release**: a small fixed pool of
  `JobToken` slots (sized to max in-flight jobs, which is bounded by the worker
  pool size × max queue depth). The main thread gets a raw pointer or index;
  the pool reclaims the slot when the main thread calls `release()` after
  consuming the result. No heap allocation per job.
- **Intrusive refcount on `JobToken` itself**: if shared ownership is genuinely
  needed (e.g., a job that outlives the submitter's interest), put the refcount
  inside the token and allocate from the engine's own pool. Avoids the separate
  control block.
- **`unique_ptr` with deferred destruction**: if the worker pool only needs to
  know whether the token is still alive (not to extend its lifetime), a flag
  or atomic in the token suffices. The main thread owns the token; the worker
  writes through a raw pointer; the main thread destroys it after consumption.
  The worker must not touch the token after setting `Complete`/`Failed` — which
  is already the case in the current protocol.

The third option is the simplest and matches the actual data flow described in
§4.2. The worker's `store(..., release)` is the last write; after that, the
worker never touches the token again. `shared_ptr` is solving a problem that
may not exist.

**Priority**: low — this lands in Chunk 4 and the current design is correct,
just heavier than necessary.

### 2.2 Job queue — `std::function` allocation risk

The job queue (`std::deque<std::function<void()>>`, §5.3 of
`threading-model.md`) is pragmatically fine for the stated use case (jobs
submitted at frame start, not per-entity). However:

**Concern**: `std::function` has a small-buffer optimisation whose size varies
by implementation (typically 16–32 bytes on libstdc++/libc++, 64 bytes on MSVC).
Lambdas that capture more than the SBO threshold heap-allocate on construction.
Asset loading lambdas that capture a path string, a destination pointer, and a
token reference may exceed the threshold on some implementations.

**Recommendation**: not urgent, but when implementing in Chunk 4, measure the
actual lambda capture sizes against the target toolchain's SBO. If they exceed
it, consider a fixed-size type-erased callable (e.g., a `SmallFunction<void(),
64>` with a static assert on capture size) to make allocation behaviour
predictable across compilers.

**Priority**: low — only matters if job submission frequency increases beyond
the current "a few per frame" assumption.

### 2.3 `core::logf` truncation visibility

`core::logf` formats to a 512-char stack buffer and truncates silently
(`decisions-style.md`, QI).

**Concern**: silent truncation of a diagnostic message can eat the exact
information needed to debug a production issue. The message "could not open
`/very/long/path/to/game/directory/with/many/nested/folders/...`" becomes
"could not open `/very/long/path/to/game/directory/with/m`" — and the developer
doesn't know the message was truncated.

**Recommendation**: add a truncation indicator. Two options:

- **Minimal**: if the formatted output was truncated, overwrite the last 3
  bytes of the buffer with `...` before passing to `log()`. Cost: one
  comparison after `vsnprintf`. No allocation, no counter, fully `noexcept`.
- **Observable**: in addition to the above, increment an always-on atomic
  `log_truncation_count` in the `core::log` implementation. This counter
  appears in any future `diagnostics_dump` output. Cost: one relaxed atomic
  increment on the truncation path only (which should be rare).

Both options are consistent with the project's philosophy of "always measure,
gate the output." A truncated diagnostic is a measurement failure.

**Priority**: low — implement alongside `core/log.hpp` before Chunk 2.

### 2.4 `EngineContext` and the `memory` exemption

The `EngineContext` design (Q-2 in `decisions-architecture.md`) is clean. The
explicit exemption for `memory` (must outlive `EngineContext`) is well-reasoned
and correctly documented.

**Watch item**: as more subsystems are added, resist the temptation to grant
further exemptions. Every subsystem that lives outside `EngineContext` is a
subsystem whose lifecycle is not automatically ordered by C++ destruction
semantics. `memory` earns its exemption because it is the backing store for
everything else. If a second exemption is ever proposed, that is a design smell
— it likely means the subsystem's dependencies are wrong, not that it needs
special lifetime.

### 2.5 Error return — `std::expected` deferral is correct

The decision to defer `std::expected<T, ErrorCode>` to Chunk 2 (networking) is
the right call. Introducing it before there is a subsystem that genuinely needs
rich error discrimination would be premature. Networking is the natural first
consumer because callers need to distinguish "timeout" from "refused" from
"DNS failure."

**One note**: when defining the central `ErrorCode` enum, consider whether it
should be a flat enum or a `{subsystem, code}` pair. The debug-stats doc (§7,
open question 3) already suggests `high 16 bits = subsystem, low 16 bits =
counter ordinal` for metric IDs. Using the same decomposition for error codes
would give a consistent namespacing model across both systems.

______________________________________________________________________

## 3. Threading Model Feedback

### 3.1 The model is sound

The incremental threading introduction (Model A → B-partial → B → C) is the
correct approach. Starting single-threaded and proving the ownership boundaries
before adding threads avoids the class of bugs where "it worked on one thread"
masks a data race that only manifests under contention.

The `ThreadRole` enum with `assert_thread_role` is a lightweight contract
enforcement mechanism that will catch violations in debug builds. The decision
to keep it as a runtime assertion rather than a compile-time constraint is
pragmatic — compile-time thread-safety annotations (e.g., Clang's
`-Wthread-safety`) are valuable but require a significant annotation investment
that may not be justified at this project's scale.

### 3.2 Audio callback constraints are correctly identified

The `T_AudioCallback` rules (no allocation, no blocking, no I/O) are exactly
right. The SPSC ring buffer between decoder and callback is the standard
solution.

**One addition**: document that `T_AudioCallback` must also never call
`core::log`. The default `log` implementation wraps `console::write`, which
may involve a lock or a write syscall. An audio underrun counter (already
planned as always-on) is the correct diagnostic mechanism for the callback
thread.

### 3.3 Render thread — `RenderFrame` struct design is the critical path

The observation in §6.5 ("design the ownership boundary first; threading is
mechanical once the boundary is clean") is the single most important sentence
in the threading document. The `RenderFrame` struct will be the largest and
most complex cross-thread data structure in the engine. Getting its layout and
ownership semantics right before Chunk 10 is worth dedicated design time.

**Recommendation**: when the time comes, write a `RenderFrame` boundary spec
with the same rigour as the subsystem boundary specs. Define exactly which
fields are value-copied vs. pointer-referenced, what the lifetime of referenced
data is, and what happens when the main thread modifies entity state between
frame swaps.

### 3.4 `dev_worker_threads` cvar — chicken-and-egg

The worker pool is started at `Host::init()` (Chunk 3/4). The pool size is
overridden by a `dev_worker_threads` cvar. But `cmd_cvar` is Chunk 1 and the
cvar system is already running by the time the pool starts.

**Potential issue**: if the cvar is `FCVAR_LATCH` (only takes effect on restart),
changing it mid-session does nothing — which is fine. But if it is live-mutable,
what happens? Resizing a thread pool at runtime is non-trivial. Document
explicitly that this cvar is read-once at pool creation and changes require a
restart, or implement it as `FCVAR_LATCH`.

______________________________________________________________________

## 4. Boundary Spec Feedback

### 4.1 cmd_cvar

**Strengths**: the two-queue privilege model is well-documented, the observer
pattern for breaking the circular server/client dependency is the right
solution, and the `ICompatPolicy` isolation of GoldSrc quirks is elegant.

**Feedback**:

- **D8 (lock-free reads)**: storing `value` as `std::atomic<float>` and
  atomically updating the string pointer is clever, but be explicit about the
  consistency guarantee. A reader on a non-main thread may see a new `value`
  and an old `string`, or vice versa. If this is acceptable (and for most cvars
  it is — the reader will get a consistent view on the next poll), document it.
  If any consumer needs `value` and `string` to be consistent with each other,
  that consumer needs the `shared_mutex` path.

- **Stale `cvar_t*` after `Cvar_Unlink`**: the `PrepareToUnlink` mechanism is
  a landmine. A game DLL that caches a `cvar_t*` (which the ABI explicitly
  encourages via `pfnCVarGetPointer`) and then accesses it after its own unload
  is undefined behaviour. This is a legacy constraint you cannot fix, but
  document it as a known hazard with a "do not use after DLL unload" annotation
  on the ABI function.

### 4.2 filesystem

**Strengths**: the decision to integrate as a static module (dropping the
`GetFSAPI` plugin boundary) simplifies the build and eliminates the injected
callback indirection. The thread-safety decision (reader-writer lock on search
paths) is correct.

**Feedback**:

- **`XASH_REDUCE_FD` and simultaneous compressed files**: this is called out as
  "silently broken" in the quirks section. If the rewrite targets any
  `REDUCE_FD` platform (PSVita, Switch), this needs a fix or an explicit
  "not supported" declaration. A single-fd LRU that evicts compressed files
  mid-stream will produce corrupted reads. Options: (a) buffer the entire
  compressed file into memory on open (feasible for small assets), (b) refuse
  to open a second compressed file while one is active, or (c) drop
  `REDUCE_FD` support entirely and document the minimum fd budget.

- **`fs_ext_path` global flag**: the "callers must reset it" pattern is a
  classic bug source. The rewrite should replace it with a scoped RAII guard
  (`ScopedDirectPath`) that resets in its destructor, or — better — make it a
  parameter to the lookup function rather than global state. This is a low-cost
  fix that eliminates an entire class of "flag left set" bugs.

- **`gameinfo_t` location**: the query-API approach (hide behind
  `const GameInfo&`) is the right choice. One refinement: make `GameInfo` a
  value type that is cheaply copyable (it's a small struct of strings and
  flags). This allows the main thread to snapshot it at frame start and pass
  the snapshot to worker threads without holding a lock. This aligns with the
  threading model's "compute and commit are separate phases" rule.

### 4.3 memory

**Strengths**: the boundary spec clearly identifies the frozen ABI constraints
(`poolhandle_t` width, sentinel constants, per-plugin pool-less API surface).

**Feedback**:

- **Pool slot reuse / stale handle aliasing** (open question 4): this is the
  highest-priority open question in the memory spec. A generation counter is
  the standard solution: pack a generation number into the upper bits of the
  handle (e.g., 8 bits generation + 24 bits slot index, or 16+16 if 256
  generations feels tight). On slot reuse, increment the generation; on lookup,
  compare generations. This turns a silent aliasing bug into a detectable
  stale-handle error. The 32-bit constraint is preserved. Cost: one comparison
  per handle dereference.

- **`std::pmr::memory_resource`** (open question 1): probably not worth it.
  PMR's value proposition is interop with `std::pmr::string` and
  `std::pmr::vector`, but the project deliberately avoids `std::string` in
  hot paths and uses `string_view` / `span` throughout. The template complexity
  and `<memory_resource>` dependency are not justified by the narrow set of
  types that would benefit.

- **Thread safety** (open question 2): given the threading model's design, a
  per-pool mutex (not global) is the right granularity. The memory subsystem
  is documented as "must outlive `EngineContext`" and is called from any thread.
  A global lock serialises all allocations across all pools; a per-pool lock
  allows independent pools to allocate concurrently. For the worker pool use
  case (asset loading), each worker can allocate from its own pool without
  contention. Cost: one `std::mutex` per pool slot (not per allocation).

### 4.4 platform

**Strengths**: the API is appropriately thin — OS wrappers that return by value,
no pool dependencies, no lifecycle management beyond `crash::install_handler`.
The `OsFd` RAII wrapper is correct.

**Feedback**:

- **`console::read_line()` static buffer**: returning a `string_view` into a
  static buffer that is overwritten on each call is a classic lifetime hazard.
  The threading model says `read_line` is main-thread-only, which mitigates the
  race condition, but the "valid until next call" contract is still fragile.
  Consider returning a small stack-allocated string (e.g., `FixedString<256>`)
  by value instead. The copy cost is negligible for a function called at most
  once per frame, and it eliminates the dangling-view risk entirely.

- **Clipboard** (open question): put it in platform. Clipboard is an OS
  primitive, not a window-system primitive. The console paste use case is the
  immediate consumer; deferring it to a `window` subsystem couples it to SDL
  or a renderer, which is unnecessary.

- **SIGTERM** (open question): own it in platform, translate to a flag. Platform
  catches the signal and sets an atomic `quit_requested` flag; the host loop
  checks the flag at frame start. This keeps signal handling in the OS
  abstraction layer where it belongs, and avoids platform having a dependency on
  host.

### 4.5 public-utilities

**Strengths**: the boundary spec correctly identifies that the only ABI
exposure is four CRC32 function pointers in `enginefuncs_t`, and that
everything else is internal.

**Feedback**:

- **`matrixlib` → `com_model.h` dependency** (open question 1): break it.
  The matrix library should take `const float*` or `std::span<const float>`
  for bone/attachment data, not SDK struct types. This makes `public/` fully
  independent of `common/` and allows the matrix functions to be unit-tested
  without pulling in the SDK headers. The SDK struct types can be cast to
  `const float*` at the call site — they are layout-compatible by construction.

- **`getopt` necessity** (open question 5): drop it. A simple `argc`/`argv`
  parser using `string_view` is 30 lines of C++17 and eliminates the POSIX
  global state (`optind`, `optarg`, etc.) which is not thread-safe and not
  needed. No game DLL calls `getopt` — it is only used by the engine's own
  `main()`.

- **`miniz` vendor strategy** (open question 4): vendor it in
  `xash3dpp/3rdparty/`. System zlib introduces a version-mismatch risk and
  a symbol-collision risk. A vendored copy is one file, version-pinned, and
  built with the same compiler flags as the rest of the engine. The filesystem
  boundary spec already lists `xash3dpp_miniz` as a PRIVATE dependency —
  vendoring is the natural home.

- **`Q_floor`/`Q_ceil` cast-through-int**: add a comment at the definition
  site and a test that exercises the negative-number behaviour. This is the
  kind of subtle semantic difference that will be "fixed" by a well-meaning
  contributor who doesn't read the quirks section.

______________________________________________________________________

## 5. Cross-Cutting Observations

### 5.1 The documentation is the architecture

The most valuable artefact in this project is not the code — it is the decision
documents and boundary specs. They encode the *why* behind every choice, which
is the information that decays fastest in a codebase. The "grandfathered /
bring into conformance when next touched" policy only works if the documents
are kept current. Treat them as living documents: when a decision is revisited,
update the doc, don't just change the code.

### 5.2 The three-tier stats model should be a template

The always-on / `XASH_STATS` / `XASH_DEBUG_*` pattern is proven in cmd_cvar
and documented in `debug-stats-design.md`. When the third subsystem
(`filesystem` or `networking`) gets its stats struct, consider extracting the
common infrastructure (the compile-time guards, the `stats()` accessor pattern,
the always-on atomic counter idiom) into a small header or documented recipe.
Not a framework — just a pattern that new subsystems can copy without
reinventing the guard macros.

### 5.3 `@thread-safety` annotation rollout

The threading model mandates `// @thread-safety:` annotations on all new public
APIs but notes zero adoption today. This is a documentation debt that
accumulates invisibly. Consider adding a CI check (even a simple `grep`) that
flags new public header functions without the annotation. A lint that catches
omissions at PR time is more reliable than a reviewer checklist.

### 5.4 The `XASH_GOLDSRC_COMPAT` link-time selection is elegant

Zero `#ifdef` in core logic, quirks isolated to a separate TU behind an
interface, CMake option selects real-or-stub at link time. This is the gold
standard for feature toggles in C++ and should be preserved as the pattern for
any future build-time variant.

### 5.5 Test framework — the doctest trigger is reasonable

The hand-rolled `CHECK`/`REQUIRE` set with a "revisit at ~15 test files" trigger
is pragmatic. One pre-emptive note: when the trigger fires, evaluate
**Catch2** alongside doctest. Catch2 v3 is header-only, supports
`CATCH_CONFIG_DISABLE_EXCEPTIONS`, and has a larger ecosystem. Both are
reasonable choices; the point is to evaluate more than one option.

______________________________________________________________________

## 6. Summary of Recommended Actions

### Blocking (before the relevant chunk ships)

| Item | Chunk | Recommendation |
|------|-------|----------------|
| `core::logf` truncation indicator | Pre-Chunk 2 | Add `...` suffix and optional truncation counter |
| `dev_worker_threads` latch semantics | worker pool (unscheduled) | Document as read-once or implement `FCVAR_LATCH` |
| `T_AudioCallback` must not call `core::log` | Chunk 9 (sound) | Add to threading model §3.4 constraints |

### Recommended (when next touched)

| Item | Subsystem | Recommendation |
|------|-----------|----------------|
| `shared_ptr<JobToken>` → simpler ownership | threading | Evaluate `unique_ptr` + raw-pointer worker access |
| `fs_ext_path` global flag → scoped guard or parameter | filesystem | Eliminate "flag left set" bug class |
| Pool slot generation counters | memory | Detect stale handle aliasing |
| `console::read_line` → return by value | platform | Eliminate dangling `string_view` risk |
| `matrixlib` decouple from `com_model.h` | utilities | Take `const float*` / `span` instead of SDK types |
| Drop `getopt` | utilities | Replace with 30-line `string_view` parser |

### Low priority (new code only / opportunistic)

| Item | Recommendation |
|------|----------------|
| `std::function` in job queue | Measure capture sizes; consider fixed-size callable if SBO is exceeded |
| `std::pmr::memory_resource` | Skip — not justified by current type usage |
| CI lint for `@thread-safety` annotations | Add when annotation count reaches critical mass |
| Stats pattern extraction | Extract common infrastructure when third subsystem adopts it |
| `Q_floor`/`Q_ceil` negative-number test | Add alongside any `crtlib` test work |
| `D8` atomic cvar consistency | Document that `value` and `string` may be transiently inconsistent across threads |

______________________________________________________________________

## 7. Open Questions — Opinions

Where the boundary specs pose open questions and I have a recommendation:

| Spec | Question | Recommendation | Rationale |
|------|----------|----------------|-----------|
| memory | `std::pmr`? | No | Template complexity not justified; `pool_ptr` is sufficient |
| memory | Thread safety model? | Per-pool mutex | Allows concurrent allocation across independent pools |
| memory | Drop `MEM_SMALL_ALLOC_OPT`? | Yes, if a slab allocator replaces it | The 24-byte saving per allocation is not worth the dual-header complexity in modern memory budgets |
| memory | `XASH_CUSTOM_SWAP`? | Drop unless a specific embedded target is planned | Keep the abstraction point (injectable backing allocator) but don't maintain dead code |
| memory | Stale handle safety? | Generation counter in upper handle bits | Standard solution; 32-bit constraint preserved |
| memory | Pool-less plugin API? | Keep the pattern | Simplicity for plugin authors outweighs the internal awkwardness |
| filesystem | `gameinfo_t` location? | Query API (`const GameInfo&`) | Copyable value type; snapshot-friendly for worker threads |
| filesystem | Case-insensitive lookup? | Skip on case-insensitive volumes; lazy trie otherwise | Best of both worlds; the legacy detection logic is already written |
| platform | Clipboard? | Platform owns it | OS primitive, not window-system |
| platform | SIGTERM? | Platform catches, sets atomic flag | Host polls; no reverse dependency |
| utilities | `matrixlib` → `com_model.h`? | Break the dependency | Take `const float*`; cast at call site |
| utilities | CRC32 binding? | Inline lambdas at the `enginefuncs_t` fill site | Keeps utilities clean; binding is a one-liner |
| utilities | `utflib` codepage tables? | Keep CP1251/CP1252 for now | Legacy mod compat; removal is a separate decision |
| utilities | `miniz` vendor strategy? | Vendor in `3rdparty/` | Version-pinned, no symbol collision risk |
| utilities | Drop `getopt`? | Yes | Replace with trivial `string_view` parser |
| utilities | `Q_timestamp` thread safety? | Replace with `localtime_r`/`localtime_s` | Trivial fix; do it when the file is next touched |
| debug-stats | Frame counter source? | `diagnostics::current_frame()` backed by relaxed atomic | Updated by host loop; readable from any thread without coupling |
| debug-stats | Consumer auth for TCP channel? | Startup-generated single-use token | Simpler and more secure than shared password |
| debug-stats | Metric ID scheme? | `{subsystem_enum, counter_ordinal}` — 16+16 bit | Matches proposed `ErrorCode` decomposition |
| debug-stats | Tracy/Optick integration? | Note as future path, don't design for it now | Ring buffer + serialiser is compatible; no premature abstraction needed |
