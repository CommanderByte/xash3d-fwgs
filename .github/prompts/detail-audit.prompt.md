---
name: "Detail audit — structural compliance"
description: "Deep structural audit of an xash3dpp module for the 'itty bitty' details that sweep-module.prompt.md does not check: limits.hpp magic-number coverage, header placement (public/private/stats split), memory subsystem integration (correct pool, correct lifetime), stats tiering (three-tier model), dependency injection completeness, and compat isolation. Read-only analysis phase produces a violations table; fix phase applies only what the audit identified. Commits when clean."
argument-hint: "module name, e.g. 'networking', 'cmd_cvar', 'filesystem'"
agent: agent
tools: [read, search, edit, run, terminal]
mode: agent
model: claude-sonnet-4-6
---

# Detail Audit: `$ARGUMENTS`

Perform a **structural and architectural deep-audit** of the `$ARGUMENTS` module.
This prompt covers checks that `sweep-module` does not perform — specifically:
limits.hpp coverage, header placement, memory subsystem integration, stats tiering,
dependency injection completeness, and compat isolation.

Work autonomously from analysis through to commit. Do not ask for confirmation.

---

## Guardrails

- **Only fix what the audit explicitly identifies.** No opportunistic improvements.
- **Do not refactor logic, rename symbols, or reorder declarations** beyond what a
  structural rule requires.
- **Do not add docstrings or comments** to code you did not change for a rule reason.
- **If a file has zero violations, do not touch it.**
- **Stop at the module boundary.** Changes in another module's files require a
  separate audit for that module.
- This prompt does **not** re-check style rules (NODISCARD, NAMING_FN, ASSERTIONS,
  etc.) — run `sweep-module` for those.

---

## Reference Documents

Read these before auditing. The specific sections are called out in each check.

1. `xash3dpp/include/xash3dpp/limits.hpp` — canonical location for all fixed limits
2. `xash3dpp/docs/design/decisions-architecture.md` — DI_PARAMS (Q-4), OWNERSHIP (Q-9), ALLOC_POLICY (Q-13), STATS_TIERS, Q-11/Q-12
3. `xash3dpp/docs/design/debug-stats-design.md` — three-tier model, exemption criteria
4. `.github/instructions/xash3dpp.instructions.md` — header placement rules, pool/memory rules

---

## Step 1 — Inventory

List every file in the module:

- `xash3dpp/include/xash3dpp/$ARGUMENTS/` — public headers
- `xash3dpp/include/xash3dpp/private/$ARGUMENTS/` — private headers
- `xash3dpp/src/$ARGUMENTS/` — implementation files
- `xash3dpp/tests/$ARGUMENTS/` — test files

Also note the limits block for this module in `xash3dpp/include/xash3dpp/limits.hpp`.

---

## Step 2 — Audit

Work through every check below. For each violation, record a row in the violations
table (format at end of this section). Record every violation before making any
changes.

---

### CHECK-LIMITS — Magic number coverage in `limits.hpp`

**Rule**: all fixed buffer sizes, pool capacities, count limits, and wire-protocol
constants that appear as numeric literals in subsystem headers or source files
must instead be named `inline constexpr` values in `xash3dpp/include/xash3dpp/limits.hpp`,
grouped under a `// $ARGUMENTS subsystem` comment block, with an
`#ifndef XASH_LIMIT_<NAME>` / `#else` override pattern.

**How to audit**:
1. Read every header and source file in the module.
2. Find any numeric literal that is:
   - A buffer or array size (used in `std::array<T, N>`, `std::vector.reserve(N)`,
     loop bounds, ring-buffer indices)
   - A wire-protocol constant (magic byte sequences, packet type identifiers,
     fragment-count limits)
   - A capacity or threshold (pool slots, max retries, timeout values)
3. Check whether each such literal is already referenced via `limits.hpp`.

**Flag**:
- Numeric literal that should be a named limit but is not in `limits.hpp` → **WARNING**
- Duplicate of an existing limits.hpp constant expressed as a raw literal → **WARNING**
- `limits.hpp` constant that lacks the `XASH_LIMIT_<NAME>` override macro → **WARNING**

**Exempt**:
- Literals that are mathematical constants (0, 1, 2, powers of 2 used in bit ops)
- GoldSrc ABI literals that are frozen in a legacy header
- Literals that appear only in test files (not subsystem code)

---

### CHECK-HEADERS — Header placement (public / private / stats split)

**Rule** (from `xash3dpp.instructions.md`):
- **Public headers** (API exposed to other subsystems): `xash3dpp/include/xash3dpp/$ARGUMENTS/`
- **Private headers** (impl details shared between TUs of the same subsystem):
  `xash3dpp/include/xash3dpp/private/$ARGUMENTS/`
- **No `.hpp` files under `src/`** — everything goes under `include/`.
- **Stats struct placement**: a subsystem's `<Subsystem>Stats` struct may live inline
  in `context.hpp` or in a dedicated `stats.hpp` in the public include tree. It must
  NOT be buried in a private header (consumers need to read stats without touching
  implementation details).
- **Debug/trace types** (ring buffer, change log, histogram) guarded by
  `#if XASH_DEBUG_<SUBSYSTEM>` belong in the private tree — they are not part of
  the public API.
- **Compat/driver headers** (`I<X>CompatPolicy`, `IProtocolDriver`) belong in the
  private tree unless they are part of the injectable interface used by callers.

**How to audit**:
1. List every `.hpp` file in the module.
2. For each file, determine: is it part of the public API? Is it implementation-detail?
   Is it a stats/debug type?
3. Verify it is in the correct directory tier.

**Flag**:
- `.hpp` file under `src/` → **BLOCKER**
- Implementation-detail header (only included by files in `src/$ARGUMENTS/`) in the
  public include tree → **WARNING** (should move to `private/`)
- `<Subsystem>Stats` struct accessible only through a private header → **WARNING**
- Debug/trace types (change logs, histograms, `#if XASH_DEBUG`) in public header → **WARNING**

---

### CHECK-MEMORY — Memory subsystem integration

**Rule** (from `xash3dpp.instructions.md` "Use Existing Framework Primitives"):

| Need | Required | Forbidden |
|------|----------|-----------|
| Heap allocation of subsystem-owned objects | `memory::pool_new<T>`, `memory::mem_alloc` | `new T(`, `malloc` |
| Deallocation | `memory::mem_free`, pool destructor | `delete`, `free` |
| Pimpl construction | `std::make_unique<Impl>()` | `new Impl(` directly |
| Short-lived scratch (frame/session) | frame pool or session pool | long-lived pool |
| Process-lifetime objects | engine lifetime pool | per-frame allocator |

**How to audit**:
1. Find every allocation in the module (`new `, `std::make_unique`, `std::vector`,
   `malloc`, `calloc`).
2. For each:
   - Is `std::make_unique` used for anything other than pimpl construction? Flag.
   - Is `new T(` used outside `std::make_unique`? Flag as BLOCKER.
   - Is any `std::vector` or `std::deque` class member in a hot-path class body
     (called per-frame) missing a `// @pre-reserved: <LIMIT_NAME>` comment? (Q-13
     ALLOC_POLICY: hot-path containers must `.reserve(N)` at init using a `limits.hpp`
     constant. Warm-path and cold-path containers are exempt — see Q-13 for the
     three-tier classification.)
   - If a `// @pre-reserved:` annotation is present, is the matching `.reserve()`
     call present in the class `init()` or constructor?
   - Is a long-lived object allocated with a short-lived pool or vice versa? Flag.
3. Check that the `*InitParams` struct for this subsystem includes a `MemoryPool*`
   or `MemoryContext*` parameter if the subsystem performs any non-trivial allocation.

**Flag**:
- `new T(` outside `std::make_unique<Impl>()` → **BLOCKER**
- `malloc`, `calloc`, `free`, `delete` → **BLOCKER**
- `std::make_unique<T>` where T is not a pimpl Impl → **WARNING** (migrate to pool_new when pool exists)
- Hot-path `std::vector`/`std::deque` class member without `// @pre-reserved: <LIMIT_NAME>` → **WARNING** (Q-13)
- Pre-reserve annotation present but no `.reserve()` in init/constructor → **WARNING** (Q-13)
- Custom `PoolAllocator<T>` implementation before migration trigger is met → **BLOCKER** (Q-13)
- Module allocates long-lived objects but no pool handle in `InitParams` → **WARNING**

---

### CHECK-STATS — Stats tiering compliance

**Rule** (from `debug-stats-design.md` §6.1 and `xash3dpp.instructions.md`
§Stats and Debug Instrumentation):

Every subsystem with non-trivial mutable runtime state must:
1. Define a `<Subsystem>Stats` struct.
2. Expose `const <Subsystem>Stats& stats() const noexcept` on its context class.
3. Place always-on counters (≤ 1 relaxed atomic per event) outside any `#if` guard.
4. Place bookkeeping counters (`#if XASH_STATS`) in the `Stats` struct under guard.
5. Place heavy tracing (`#if XASH_DEBUG_<SUBSYSTEM>`) in the `Stats` struct under guard.
6. Never gate a counter increment on a runtime boolean — always measure.
7. Never format strings inside a hot-path counter increment.

**Exemptions** (both conditions must hold):
- Pure function namespace with **no** internal mutable state, OR
- Init-once-at-startup utility that never mutates after `init()`.

Call frequency is **not** grounds for exemption.

**How to audit**:
1. Does the subsystem have mutable runtime state? (pimpl with non-const fields that
   change during normal operation)
2. If yes: does a `<Subsystem>Stats` struct exist? Is it accessible from public headers?
3. Is `stats() const noexcept` declared and implemented?
4. Are counter increments unconditional (not wrapped in `if (debug_enabled)`)?
5. Are any counter increments followed immediately by string formatting? Flag.

**Flag**:
- Mutable stateful subsystem with no `Stats` struct → **WARNING**
- `Stats` struct exists but `stats()` accessor is missing → **WARNING**
- Counter increment inside `if (runtime_bool)` → **WARNING**
- `snprintf` / string formatting adjacent to a counter increment in a hot path → **WARNING**
- Heavy tracing (ring buffer, change log) with no `#if XASH_DEBUG_*` guard → **WARNING**

---

### CHECK-DI — Dependency injection completeness

**Rule** (from `decisions-architecture.md` §DI_PARAMS (Q-4)):

Every dependency that a subsystem needs at runtime must flow through its
`<Subsystem>InitParams` struct. A subsystem must never pull a dependency from a
process-global (other than the `memory` singleton, which is the documented exception).

**How to audit**:
1. Read every `.cpp` file in the module for references to globals in other subsystems
   (e.g. `g_filesystem`, `g_engine`, a module-level `CmdCvarContext*` set at startup).
2. Check that `InitParams` includes a field for every external object the subsystem
   calls into (filesystem, cvar context, logger, platform sockets, etc.).
3. Verify that `InitParams` fields are stored in the pimpl struct, not at file scope.

**Flag**:
- Reference to a mutable global from another subsystem → **BLOCKER**
- External dependency called through a file-scope pointer/reference set in `init()` → **WARNING**
  (acceptable if documented as "temporary until DI wiring is complete", but flag for tracking)
- `InitParams` struct does not include a dependency the subsystem clearly uses → **WARNING**

---

### CHECK-COMPAT — Compat isolation

**Rule** (from `xash3dpp.instructions.md` §Compat isolation and `decisions-architecture.md` §Q-12):

- No `#ifdef XASH_GOLDSRC_COMPAT` or similar guards in **core logic** files.
- Compat variants live in separate `.cpp` files selected at link time by CMake option.
- No engine-wide unified compat policy type (`IEngineCompatPolicy`, `GlobalCompatPolicy`).
- Each subsystem that has compat variants defines its own `I<X>CompatPolicy` or
  `I<X>Driver` in the private header tree.

**How to audit**:
1. Grep for `#ifdef XASH_GOLDSRC_COMPAT` and `#if.*COMPAT` in core logic files
   (not in CMakeLists.txt, which is the correct location for the switch).
2. Check that every compat seam uses a link-time-selected `.cpp` file pair.
3. Check for any engine-wide compat struct that aggregates policies from multiple subsystems.

**Flag**:
- `#ifdef XASH_GOLDSRC_COMPAT` in a `.cpp` that is not a `compat_*.cpp` file → **WARNING**
- Engine-wide compat policy aggregating multiple subsystems → **BLOCKER**
- Compat variant inlined via `if (is_goldsrc_)` member at runtime in hot path → **WARNING**

---

### Violations table format

Record each finding as:

| # | File | Line | Check | Severity | Notes |
|---|------|------|-------|----------|-------|
| 1 | ... | ... | CHECK-LIMITS | WARNING | `net_max_fragments = 506` raw literal; should be `::xash::limits::net_max_fragments` |

Print the complete table before making any changes.

---

## Step 3 — Fix

Apply every fix identified in Step 2. Work through BLOCKERs first, then WARNINGs.
Batch independent edits with `multi_replace_string_in_file`.

**Fix patterns**:

- **CHECK-LIMITS — add to limits.hpp**:
  1. Open `xash3dpp/include/xash3dpp/limits.hpp`.
  2. Find or create the `// $ARGUMENTS subsystem` comment block.
  3. Add `#ifndef XASH_LIMIT_<NAME>` / `inline constexpr <type> <name> = <value>;` / `#endif`.
  4. In the original file, replace the literal with `::xash::limits::<name>`.

- **CHECK-HEADERS — move a private header**:
  1. Move the `.hpp` from `include/xash3dpp/$ARGUMENTS/` to `include/xash3dpp/private/$ARGUMENTS/`.
  2. Update all `#include` paths in `.cpp` files that include it.
  3. Verify no public header now transitively exposes the moved header.

- **CHECK-MEMORY — replace forbidden allocation**:
  - Replace `new T(args)` (non-pimpl) with `memory::pool_new<T>(pool, args)` when the
    pool handle is available. If the pool handle does not yet exist in `InitParams`,
    mark the change as `TODO: wire pool` and convert to `std::make_unique<T>` as a
    temporary measure (record as WARNING in the violations table).
  - Replace `delete ptr` with `memory::mem_free(ptr)`.

- **CHECK-STATS — add stats struct**:
  Add a minimal `<Subsystem>Stats` struct to the context header. Start with just the
  counters that already exist in the code (bytes in/out, packets sent/received, etc.).
  Do not add counters that do not yet have corresponding code paths — the struct should
  reflect existing measurements, not aspirational ones.

- **CHECK-DI — fix dependency access**:
  Add the dependency to `InitParams`. Store it in the pimpl struct. Replace the
  global/file-scope access with the stored pointer. This may require updating call
  sites that construct the `InitParams` struct — limit changes to the module under audit.

---

## Step 4 — Build and test

```powershell
cmake -S xash3dpp -B build
cmake --build build --config Debug 2>&1 | Select-String "error C[0-9]|error:"

ctest --test-dir build -C Debug --output-on-failure -R "$ARGUMENTS" 2>&1 | Select-Object -Last 20
```

If there are build errors, trace them to the fix that caused them, correct, and rebuild.
Do not weaken tests.

---

## Step 5 — Commit

Stage only `xash3dpp/` changes:

```powershell
cd "c:\git\xash3d-fwgs"
git add -A xash3dpp/
git diff --cached --name-only | Where-Object { $_ -notlike "xash3dpp/*" }
```

If that second command produces any output, unstage those files.

Commit message format:

```
refactor($ARGUMENTS): detail audit — limits, headers, memory, stats, DI

- <one line per concrete change, grouped by CHECK-* category>
```

Verify the commit landed by reading `.git/refs/heads/<branch>` directly.

---

## Done Condition

Task is complete when:
1. All BLOCKER violations are resolved.
2. All WARNING violations are either fixed or explicitly accepted with a
   `// detail-audit: accepted — <reason>` comment.
3. `100% tests passed`.
4. The commit is on the branch.
5. A summary is printed listing every check, how many violations were found, and
   how each was resolved.
