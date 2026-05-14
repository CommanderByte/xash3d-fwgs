# Utilities Threading Analysis

> Boundary spec: `docs/boundaries/public-utilities-boundary.md`

## Ownership model

The utilities library is a **stateless helper library**.  It owns no thread
objects, no mutexes, and no engine lifecycle.  Almost every public function is a
pure transformation of its arguments: callers supply all input, all output goes
to caller-provided buffers or return values, and no module-level data is written
during normal operation.

There is one exception — `build::number()` — discussed under Hazards.

**Expected callers**: any thread in the engine.  No enforcement exists and none
is needed for the stateless functions.  `Atlas` instances are expected to be
owned by a single thread (typically the renderer/upload thread) and must not be
shared across threads without external locking.

## Safe items

- **`string::{strncpy,strnicmp,stricmp,snprintf,vsnprintf,atoi,atof,atov,strip_colors,match_pattern,parse_token}`**
  — Pure functions operating entirely on caller-supplied buffers and
  `string_view`s.  No shared state.  Safe for concurrent calls from any thread.

- **`string::pretify_mem`** — Returns a `std::string` built from local temporaries.
  No shared state.  Heap allocation is thread-safe under the standard allocator.

- **`string::Tokenizer`** — All mutable state (`cursor_`, `flags_`, `buf_`) lives in
  the instance.  Caller-owned.  Safe provided the instance is not shared.

- **`path::{file_extension,filename,file_base,strip_extension,fix_slashes,extract_dir,default_extension,replace_extension,remove_line_feed,trim_space}`**
  — Pure functions.  `find_extension` is a file-scope `static` helper function,
  not a data variable.  No shared state.

- **`hash::{crc32_*,md5_*,crc32_block_sequence}`** — All state is passed through
  caller-supplied `Crc32State`/`Md5State` structs.  No file-scope mutable data
  found in `hash.cpp`.  Safe for concurrent use by independent callers.

- **`matrix::{transform_point,rotate_vector,concat,invert_ortho,from_angles,angle_vectors,vec_to_yaw,vector_angles}` and all `math.hpp` inlines**
  — Pure functions on value types.  No shared state.

- **`utf::{decode_utf8,decode_utf16,encode_utf8,length,utf16_to_utf8,to_cp1251,to_cp1252}`**
  — `DecodeState` is caller-owned.  `k_cp1251_table` is `static constexpr` (read-only).
  `codepoint_length` is a file-scope `static` helper function.  No shared state.

- **`utf::{Utf8Decoder,Utf16Decoder}`** — State lives in the instance.  Caller-owned.

- **`dynlib::{clear_exports,validate_exports}`** — Operate on caller-supplied spans
  of `ExportEntry`.  The pointed-to function-pointer slots (`void **`) are owned
  by the caller; it is the caller's responsibility to ensure no other thread reads
  those slots while a write is in progress.

- **`build::{commit,branch,commit_date}`** — `[[gnu::weak]] const std::string_view`
  symbols.  Written once at link time (or provided by the generated `build_vcs.cpp`
  object).  After program start these are read-only.  **Safe-RO**.

- **`build::COMPAT_NUMBER`** — compile-time constant.  **Safe-RO**.

- **`Atlas::Atlas`, `Atlas::clear`, `Atlas::size`, `Atlas::max_height`** — These
  access only instance state.  Safe provided the instance is not shared across
  threads.

## Hazards

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `static const int cached` inside `build::number()` | `build.cpp:69` | Race-lazy-init (mitigated) | C++11 magic static. Thread-safe by the standard **when** the runtime's static-init guard is present. CMakeLists.txt does **not** pass `-fno-threadsafe-statics` (GCC/Clang) and targets MSVC ≥ 2015, so the guard is active. Becomes a real data race if the project is ever compiled with `-fno-threadsafe-statics`. |
| `Atlas::m_allocated`, `Atlas::m_max_height` | `atlas.cpp` | Race-shared (caller responsibility) | Instance-level mutable arrays. Multiple threads calling `alloc()` on the **same** `Atlas` object concurrently will corrupt the strip-height table. This is safe in the expected single-owner usage model but is not enforced by the class. |

## Required caller contracts

1. **`Atlas`** — One instance must not be called from more than one thread
   simultaneously.  If the instance is shared (e.g. a global lightmap atlas
   updated by multiple upload threads), the caller must acquire an external lock
   around every call to `alloc()` and `clear()`.

2. **`dynlib::{clear_exports,validate_exports}`** — The function-pointer slots in
   the table must not be read from another thread while `clear_exports` is
   writing them, and must not be read by the engine's call dispatch while
   `validate_exports` is scanning them.  Typically satisfied because both
   functions are called only during plugin load/unload, which happens on the
   main thread before other threads start using the plugin.

3. **`build::number()`** — Must not be called from a context compiled with
   `-fno-threadsafe-statics`.  Specifically: do not call it from a static
   object constructor that may race with other TUs at program startup.

## Recommendations

1. **Add a `static_assert` or comment to `build::number()`** noting the
   magic-static dependency:

   ```cpp
   // Thread-safety relies on C++11 magic-static guards.
   // Do not compile this TU with -fno-threadsafe-statics.
   ```

2. **Document the Atlas single-owner contract in `atlas.hpp`** with a brief
   `// Not thread-safe: one owner thread only` comment on the class, matching
   the pattern used by legacy `atlas.h`.

3. **No synchronisation should be added to the library itself.**  These are
   low-level utilities that are intentionally lock-free.  Any needed locking
   belongs in the subsystem that owns the mutable object (`Atlas`, export
   table, etc.).

4. **No signal-safety concern identified.**  None of the utility functions are
   called from signal handlers in the legacy engine.  `pretify_mem` and the
   `std::string`-returning path helpers perform heap allocation and must not be
   introduced into a signal-handling path.
