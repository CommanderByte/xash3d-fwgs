---
applyTo: "xash3dpp/**"
---

## xash3dpp Conventions

This tree is a self-contained C++ project, intended to become its own repository.
The legacy engine at the repo root is the behavioural reference only.

- **Language**: C++20. No exceptions, no RTTI (`/EHs-c- /GR-` on MSVC; `-fno-exceptions -fno-rtti` on GCC/Clang).
- **Build system**: CMake (not Waf). Add targets under `xash3dpp/cmake/`.
- **Tests**: go in `xash3dpp/tests/`. Mirror the subsystem path: `src/filesystem/` → `tests/filesystem/`.
- **Public headers** (API exposed to other subsystems) live in `xash3dpp/include/xash3dpp/<subsystem>/`. Private/internal headers — including implementation-detail headers shared between TUs of the same subsystem — live in `xash3dpp/include/xash3dpp/private/<subsystem>/`, mirroring the public tree. Only `.cpp` files go in `xash3dpp/src/`; do **not** put `.hpp` files under `src/`. A single CMake `PATTERN "private" EXCLUDE` rule keeps private headers out of any install target.
- **Third-party deps**: vendor under `xash3dpp/3rdparty/`. Do not reuse the repo-root `3rdparty/`.
- **Design notes** for a subsystem go in `xash3dpp/docs/` before implementation starts.

## Before Writing Any Code

1. Read the corresponding legacy subsystem using search and file tools.
2. Write a short boundary note in `xash3dpp/docs/` covering: what the module exposes, what invariants it must preserve, and any quirks found in the legacy code.
3. Check the table below for existing framework primitives and prefer them.
4. Then implement.

## Use Existing Framework Primitives — Mandatory

Before reaching for the standard library or OS APIs, check whether the needed
functionality already exists in the framework. Using framework primitives is not
optional — it ensures consistent behaviour, avoids duplication, and keeps
pool-based memory accounting accurate.

| Need | Use | Header |
|------|-----|--------|
| String copy, compare, format, parse | `utilities::strncpy`, `stricmp`, `snprintf`, `atoi`, `parse_token`, `Tokenizer` | `utilities/string.hpp` |
| Case-insensitive compare / sort key | `utilities::ci_less`, `ci_equal` | `utilities/string.hpp` |
| Path join, extension, base name | `utilities::path_join`, `file_base`, `replace_extension`, `fix_slashes`, … | `utilities/path.hpp` |
| CRC-32 / MD5 hashing | `utilities::Crc32Hasher`, `Md5Hasher`, `crc32()` | `utilities/hash.hpp` |
| Vector / matrix math | `utilities::Vec2/3/4`, `Matrix3x4/4x4`, `dot`, `cross`, `normalize`, … | `utilities/math.hpp`, `utilities/matrix.hpp` |
| UTF-8 / UTF-16 encode/decode | `utilities::utf::Utf8Decoder`, `encode_utf8`, `utf16_to_utf8` | `utilities/utf.hpp` |
| Dynamic allocations (pool-backed) | `memory::mem_alloc`, `mem_calloc`, `mem_free`, `pool_new<T>`, `pool_dup` | `memory/memory.hpp` |
| File open / read / write / search | `IFilesystem` interface injected at construction | `filesystem/filesystem.hpp` |
| Monotonic time | `platform::get_time()` | `platform/sys.hpp` |
| Console output | `platform::console::write(std::string_view)` | `platform/console.hpp` |
| Debugger detection / break | `platform::is_debugger_present()`, `XASH_DEBUG_BREAK()` | `platform/sys.hpp` |

**Forbidden raw alternatives** (flag in code review):
- `malloc`, `calloc`, `realloc`, `free` — use `memory::mem_alloc` / `mem_free`
- `new` / `delete` — use pool helpers; `std::make_unique<Impl>()` is the *only* allowed exception (pimpl construction before the subsystem pool exists)
- `fopen`, `fclose`, `FILE *`, `CreateFile`, `open()` — use `IFilesystem`
- `strlen`, `strcpy`, `strcmp`, `sprintf` — use `utilities::` equivalents
- Custom CRC/MD5/hash implementations — use `utilities::hash.hpp`

## Recurring Patterns — Mandatory

### limits.hpp
Subsystem-specific buffer sizes, pool capacities, and fixed-count limits belong
in `xash3dpp/include/xash3dpp/limits.hpp`, not as magic literals in headers or
source files.  Use the `#ifndef XASH_LIMIT_<NAME>` / `inline constexpr` /
`#else` / `#endif` override pattern.  Group limits under a `// <subsystem>
subsystem` comment block.

### Pimpl move operations
When a class owns a `std::unique_ptr<Impl>`, deleting its copy constructor
also suppresses the implicit move constructor.  The correct pattern:

- **Header** (where `Impl` is incomplete): *declare* the move operations:
  ```cpp
  <Class>(<Class>&&) noexcept;
  <Class>& operator=(<Class>&&) noexcept;
  ```
- **`.cpp`** (where `Impl` is complete): *define* them:
  ```cpp
  <Class>::<Class>(<Class>&&) noexcept            = default;
  <Class>& <Class>::operator=(<Class>&&) noexcept = default;
  ```

Writing `= default` in the header triggers instantiation of
`unique_ptr<Impl>`'s destructor before `Impl` is defined, causing a
compile error in every TU that includes the header.

### Compat isolation
Use a CMake option (e.g. `XASH_GOLDSRC_COMPAT`) to select between two `.cpp`
files at link time (`compat_goldsrc.cpp` / `compat_null.cpp`).  Zero
`#ifdef` guards in core logic.

### Stats and Debug Instrumentation

All subsystems with non-trivial hot paths follow a three-tier model
(see `xash3dpp/docs/design/debug-stats-design.md` for the full analysis):

| Tier | Guard | Use for |
|------|-------|---------|
| **Always-on** | none | ≤ 1 relaxed atomic per event — live bytes, dispatch counts |
| **`XASH_STATS`** | `#if XASH_STATS` | Peak tracking, high-water marks, bookkeeping with more work per event; compiled into profiling builds, omitted in release |
| **`XASH_DEBUG_<SUBSYSTEM>`** | `#if XASH_DEBUG_<SUBSYSTEM>` | Circular change logs, break-on-write, histograms — dev builds only |

**Mandatory rules:**
- Never gate counter *increments* on a runtime boolean. Always measure; gate *output*
  at the reporting layer.
- Never format strings in a hot path. Accumulate raw numeric values; the query
  command or future serializer thread formats them on demand.
- Every non-trivial subsystem exposes `const <Subsystem>Stats& stats() const noexcept`
  on its context class. See `CmdCvarContext::stats()` as the canonical reference.

### Threading Model — Mandatory

Full specification: `xash3dpp/docs/design/threading-model.md`.

**Thread role enforcement** (every subsystem must follow):
- Call `assert_thread_role(ThreadRole::Main)` at the top of every public function
  that is main-thread-only. The call is a no-op in release builds.
- Call `register_thread_role(role)` once at the start of every background thread.
- Never call into a game DLL (`pfnThink`, `pfnClientMove`, etc.) from any thread
  other than `T_Main`.

**Interface rules for thread-safe-by-default design:**
- Query functions (reads) must take `const T&` context parameters — never read
  a mutable global from a function that may be called from a worker thread.
- Mutation is explicit at the call site: write through a non-const reference
  parameter or return a value that the caller applies. No hidden side effects
  into shared global state.
- Separate compute (reads) and commit (writes) phases in loops over entity state.
  Even when execution is sequential today, this structure enables future
  parallelism without redesign.
- `WorldData` (BSP, PVS, collision data) is immutable after activation.
  All trace/PVS/model query functions take `const WorldData&`.

**Async work via `JobToken`:**
- Background jobs (asset loading, PVS computation, HTTP I/O) run on `T_Worker`.
- Submitted via `platform::submit_job<T>(fn)`; result retrieved via
  `JobToken<T>::status` (atomic acquire load) + `std::unique_ptr<T>` move.
- Never use `std::future` (requires exceptions) or raw `condition_variable` waits
  in game-loop code for job completion checks.

**Forbidden threading patterns:**
- No `recvfrom()`/`sendto()` calls outside the networking subsystem.
- No new mutable global variables — state lives in context objects.
- No mutex inside the audio callback (`T_AudioCallback`).
- No memory allocation inside the audio callback.
- No blocking I/O or lock acquisition inside the audio callback.

### EngineContext and Dependency Injection — Mandatory

Full decisions: `xash3dpp/docs/design/design-paradigms-round1.md` Q-1, Q-2, Q-4.

- **Stateful subsystems** (those with a non-trivial lifecycle) are pimpl classes
  owned as direct members of `EngineContext` in init order. Destructor order
  provides automatic, deterministic shutdown — no explicit shutdown calls needed.
- **Stateless / pure OS wrappers** (`utilities`, `platform`) remain free functions
  in a namespace. They do not belong in `EngineContext`.
- **`memory`** is the documented singleton exception: global pool registry that
  must outlive `EngineContext`. It stays as free functions over global state.
- **No global `g_engine` accessor.** Dependencies flow in through
  `<Subsystem>InitParams` structs passed at construction, not pulled from
  a process-global.

**Every subsystem with init parameters must define a named params struct:**
```cpp
struct FilesystemInitParams {
    StringView base_dir;
    StringView game_dir;
};
// wrong: bool Init(StringView base_dir, StringView game_dir)
// right: bool Init(const FilesystemInitParams& p)
```

### Error Return Patterns — Mandatory

Full decisions: `xash3dpp/docs/design/design-paradigms-round1.md` Q-5.

| Pattern | Use for |
|---------|---------|
| `[[nodiscard]] bool` | Void-or-fail where the reason does not matter to callers |
| `[[nodiscard]] std::optional<T>` | Value may be absent — not an error, just "not found" |
| `[[nodiscard]] T*` (nullable) | Pointer return where null is the natural absent sentinel |
| `void` | Infallible operations or failure handled internally with a fallback |

- Every function returning a failure indicator **must emit a diagnostic** before
  returning (via `platform::console::write` or the logging subsystem).
  Exception: `optional<T>` returning `nullopt` for a "not found" query is silent
  by contract.
- `std::expected<T, ErrorCode>` is deferred to Chunk 2. Do **not** use it before
  the `ErrorCode` enum is defined. Do **not** call `.value()` — only
  `.has_value()` and `operator*`.
- All error return values carry `[[nodiscard]]`.

### Interface and ABI Rules — Mandatory

Full decisions: `xash3dpp/docs/design/design-paradigms-round1.md` Q-7, Q-8.

- **Intra-engine seam** (same binary, same compiler): use a C++ abstract class
  (`I<Subsystem>` vtable). Supports dependency injection and test mocking.
  `IFilesystem` is the canonical reference.
- **DLL boundary** (game DLL, client DLL, renderer DLL, menu DLL): use a C
  function-pointer struct. The legacy ABIs (`enginefuncs_t`, `DLL_FUNCTIONS`,
  `ref_api_t`) are preserved exactly and are **never changed**.
- **New plugin types** (first: Vulkan renderer at Chunk 10): versioned C
  descriptor struct with `struct_size` field and two-way version check.
  See Q-10 in the design document.
- **`std::string_view` at boundaries**: use freely within the engine binary.
  At any `extern "C"` or DLL edge, use `const char*`; wrap in `string_view`
  immediately on entry. No custom `StringRef` type.

### Ownership Vocabulary — Mandatory

Full decisions: `xash3dpp/docs/design/design-paradigms-round1.md` Q-9.

| Type | Semantics |
|------|-----------|
| `pool_ptr<T>` | Owned; deleter returns memory to the source pool |
| `std::unique_ptr<T>` | Owned; heap-backed. **Only** for pimpl before a subsystem pool exists |
| `T*` (raw) | Borrowed reference — caller must not delete; document with `// @lifetime: engine` |
| `std::span<const T>` | Default non-owning view of a contiguous range |
| `std::span<T>` | Non-owning mutable view — only when intentionally writing through |
| `std::string_view` | Non-owning string |

**Pool selection reflects lifetime**: long-lived pool for process lifetime,
session pool for `changelevel`-scoped objects, frame pool for per-frame scratch.

Raw `T*` in public APIs means "borrowed reference with engine lifetime". Document
with `// @lifetime: engine` on the declaration. No `BorrowedRef<T>` type alias.
