# Cvar Registry

> **Defined in**: `cvar.hpp`, `cvar_ops.cpp`, `private/cmd_cvar/context_impl.hpp`
> **Namespace**: `xash::cmd_cvar`

## Overview

The cvar registry stores named console variables that persist across frames. Each cvar has a name (the hash-map key), a string value (the authoritative representation), a derived float value, a flags bitmask, and additional engine-internal extensions.

The central design constraint is **ABI stability**: legacy game DLLs receive a raw `cvar_t *` pointer and dereference fields by offset. The `Cvar` struct's first member is `CvarAbi`, which is layout-identical to the legacy `cvar_t`. This is enforced by a `static_assert` at compile time.

## CvarAbi

```cpp
struct CvarAbi {
    char         *name;    // +0; pointer into pool or DLL memory
    char         *string;  // current value as null-terminated string
    uint32_t      flags;   // FCVAR_* bitmask
    float         value;   // derived float; updated on every string set
    CvarAbi      *next;    // ABI linked-list chain (Cvar_GetList)
};
```

This is the frozen layout exposed to DLLs. The field order and offsets must never change. The `next` pointer maintains a singly-linked list used only by `cvar_get_list()` for legacy DLL iteration — internal lookup always goes through `CmdHashMap`.

## Cvar (engine-internal)

`Cvar` begins with `CvarAbi abi` at offset 0, followed by engine-internal fields:

| Field | Always present | `XASH_STATS` | `XASH_DEBUG_CVARS` |
|---|---|---|---|
| `abi` (name, string, flags, value, next) | ✓ | | |
| `desc` | ✓ | | |
| `def_string` | ✓ | | |
| `generation` (`std::atomic<uint32_t>`) | ✓ | | |
| `type_hint` | ✓ | | |
| `range_min`, `range_max` | ✓ | | |
| `owner_flags` | ✓ | | |
| `write_count` (`std::atomic<uint32_t>`), `last_write_frame`, `last_write_source` | | ✓ | |
| `last_write_location` | | | ✓ |

The `static_assert(__builtin_offsetof(Cvar, abi) == 0, ...)` in `cvar.hpp` enforces that `CvarAbi` stays at offset 0 at all times.

### generation counter

`generation` is a `std::atomic<uint32_t>` incremented on every write via `memory_order_relaxed`. Consumers that need efficient change detection store the last-seen generation value and compare — avoiding `FCVAR_CHANGED` polling or any lock. This is the recommended mechanism for renderer, physics, and movevars consumers.

`FCVAR_CHANGED` is still set in `abi.flags` for legacy DLLs that poll it directly — it is maintained for ABI compatibility, not internal use.

### owner_flags

Set once at registration; used by `cvar_unlink` to remove all cvars belonging to a DLL being unloaded. Typical values map to `FCVAR_EXTDLL`, `FCVAR_CLIENTDLL`, `FCVAR_GAMEUIDLL`.

### range constraint

When `range_min <= range_max`, the float value is clamped to `[range_min, range_max]` on every `cvar_set`. Setting `range_min > range_max` (the default: `0.0f > -1.0f`) disables clamping entirely.

## CvarFlags

Standard flags frozen for DLL ABI compatibility:

| Flag | Value | Meaning |
|---|---|---|
| `FCVAR_ARCHIVE` | `1 << 0` | Saved to `config.cfg` |
| `FCVAR_USERINFO` | `1 << 1` | Sent to server as userinfo |
| `FCVAR_SERVER` | `1 << 2` | Included in serverinfo; sent to clients |
| `FCVAR_EXTDLL` | `1 << 3` | Registered by the server DLL |
| `FCVAR_CLIENTDLL` | `1 << 4` | Registered by the client DLL |
| `FCVAR_PROTECTED` | `1 << 5` | Value hidden in print (e.g. passwords) |
| `FCVAR_SPONLY` | `1 << 6` | Settable only in singleplayer |
| `FCVAR_PRINTABLEONLY` | `1 << 7` | String must be printable ASCII |
| `FCVAR_UNLOGGED` | `1 << 8` | `cvar_set` not written to log |
| `FCVAR_NOEXTRAWHITESPACE` | `1 << 9` | Leading/trailing whitespace stripped |
| `FCVAR_CHANGED` | `1 << 10` | Set by engine on write; cleared by polling DLL |
| `FCVAR_FILTERABLE` | `1 << 11` | Blocked by `cl_filterstuffcmd` when untrusted |
| `FCVAR_PRIVILEGED` | `1 << 12` | Set only from trusted (non-stuffcmd) source |
| `FCVAR_MOVEVARS` | `1 << 13` | Triggers movevars rebuild on change |
| `FCVAR_VIDRESTART` | `1 << 14` | Triggers video restart on change |
| `FCVAR_GAMEUIDLL` | `1 << 15` | Registered by the menu DLL |
| `FCVAR_USER_CREATED` | `1 << 16` | Created by `get_or_create`; not from a DLL |
| `FCVAR_DLL_WRAPPER` | `1 << 17` | Engine wraps a DLL-owned `CvarAbi *` |

## Registration paths

### Engine-owned cvars

```cpp
void CmdCvarContext::cvar_register_engine(Cvar &cv) noexcept;
```

The `Cvar` struct lives statically in engine source code, or as a member of `Impl` for the two built-in cvars (`builtin_cmd_scripting`, `builtin_cl_filterstuffcmd`). The engine passes name and default value as `const char *` literals. The registry stores a non-owning pointer to the struct; the struct must outlive the context. `def_string` is not pool-dup'd — it points to the literal directly.

### DLL-owned cvars

```cpp
[[nodiscard]] Cvar *cvar_register_dll(CvarAbi *cv) noexcept; // @lifetime: engine
```

The DLL passes a `cvar_t *` (exposed as `CvarAbi *`). The engine does not allocate a new struct — it inserts the DLL's own pointer directly into both the hash map and the ABI linked list (tagged `FCVAR_DLL_WRAPPER`). The `def_string` is pool-dup'd at registration so the engine can restore it on unlink even if the DLL frees its own storage.

### Get-or-create

```cpp
[[nodiscard]] Cvar *cvar_get_or_create(std::string_view name,
                                        const char *default_value,
                                        uint32_t flags) noexcept; // @lifetime: engine
```

If the name is already registered, returns the existing `Cvar *`. If not, allocates a new `Cvar` from the `cmd_cvar` pool, pool-dup's both the name and default string, and inserts it with `FCVAR_USER_CREATED`. Never returns `nullptr` after a successful `init()`.

## Cvar lifetime and memory

| Creation path | Struct memory | `name` string | `string` (value) string |
|---|---|---|---|
| Engine-owned | Static (`Impl` member or BSS) | Pointer to string literal | Pool-dup'd on every set |
| DLL-owned | DLL's own memory | Pointer into DLL memory | Pool-dup'd on every set |
| User-created | Pool-allocated | Pool-dup'd at creation | Pool-dup'd on every set |

On every `cvar_set`, the old `abi.string` is freed via `mem_free` and a new pool copy is stored. This means `abi.string` is always a pool allocation (except at initial registration for engine-owned cvars, which may point to a literal until first write).

## Safe DLL unlink (two-phase)

Game DLLs sometimes free their cvar structs without notifying the engine (e.g. at process exit). The two-phase unlink protocol prevents use-after-free:

1. **`cvar_prepare_to_unlink(mask)`** — called while the DLL is still loaded. Walks all cvars with `owner_flags & mask` and saves `{name (pool copy), owner_flags}` to `Impl::pending_unlink`.
2. DLL is unloaded — structs may now be invalid.
3. **`unlink_pending_cvars()`** — walks `pending_unlink`, looks up each name in the hash map, removes the node. Uses only the saved name copy, never the original `Cvar *`.

`pending_unlink` is a `std::vector<PendingUnlinkEntry>` on the system heap (not the pool) so it survives across pool lifecycles.

## Hash map and linked list

The registry maintains two parallel structures:

- **`CmdHashMap<Cvar> cvar_map`** — O(1) average case-insensitive lookup by name. Bucket count is `limits::cvar_hash_buckets` (CMake-overridable).
- **`Cvar *cvar_list_head`** — singly-linked list through `Cvar::abi.next`, used only by `cvar_get_list()` for legacy DLL iteration.

Insertions update both structures atomically on the game thread.

## See also

- [context-and-lifecycle.md](./context-and-lifecycle.md)
- [instrumentation.md](./instrumentation.md) — `CvarWriteSource`, `generation`, stats fields
- Legacy: `engine/common/cvar.c`
