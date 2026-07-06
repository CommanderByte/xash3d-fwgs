# Context and Lifecycle

> **Defined in**: `context.hpp`, `private/cmd_cvar/context_impl.hpp`, `context.cpp`, `context_init.cpp`
> **Namespace**: `xash::cmd_cvar`

## Overview

`CmdCvarContext` is the single pimpl class that owns all command-buffer and cvar-registry state. All operations the rest of the engine performs on commands and cvars go through a `CmdCvarContext` instance. There is no shared file-scope state; tests construct an isolated `CmdCvarContext` on the stack without touching any singleton.

The host layer holds a single `CmdCvarContext g_cmd_cvar` (or a pointer to one) and exposes the legacy `Cmd_*` / `Cvar_*` free-function ABI as thin shims that forward to it.

## CmdCvarContext

### Pimpl structure

```cpp
class CmdCvarContext {
    struct Impl;   // complete only in src/cmd_cvar/*.cpp

    Impl *impl_ = nullptr;

public:
    CmdCvarContext() noexcept;   // allocates Impl via pool_new(k_null_pool)
    ~CmdCvarContext();            // defined in context.cpp (Impl complete there)

    CmdCvarContext(CmdCvarContext &&) noexcept;
    CmdCvarContext &operator=(CmdCvarContext &&) noexcept;
    // copy deleted
};
```

`Impl` is forward-declared in the header and completed in `private/cmd_cvar/context_impl.hpp`, which is included only by `src/cmd_cvar/*.cpp` TUs. This prevents the pimpl body from leaking into callers.

The `Impl` is allocated via `memory::pool_new(k_null_pool)` in the constructor. `k_null_pool` is used because the subsystem's own `cmd_cvar` pool does not yet exist at this point — the pool is created inside `init()`. This means the `Impl` is an "unowned" allocation tracked outside the pool accounting system; it is freed by the destructor.

### Impl contents

The `Impl` struct holds all subsystem state:

| Member group | Members |
|---|---|
| Pool | `pool` — the subsystem memory pool (created in `init()`) |
| Injected deps | `trust_oracle`, `compat_policy` — non-owning pointers; lifetimes exceed the context |
| Observers | `observers[limits::cmd_observer_max]`, `observer_count` |
| Command queues | `cmd_text`, `filteredcmd_text` (`std::deque<std::string>`) |
| Scripting state | `cmd_wait`, `condlevel`, `cmd_condition` |
| Registries | `cvar_map`, `cvar_list_head`, `cmd_map`, `cmd_list_head`, `alias_map`, `alias_list_head` |
| Built-in cvars | `builtin_cmd_scripting`, `builtin_cl_filterstuffcmd` — `Cvar` members (engine-static, not globals) |
| Tokenizer scratch | `tok_argc`, `tok_argv`, `tok_argsBuffer`, `tok_is_privileged` — valid only during dispatch |
| DLL lifecycle flags | `server_dll_loaded`, `client_dll_loaded` |
| Pending unlink | `pending_unlink` (`std::vector<PendingUnlinkEntry>`) — system heap, drained before shutdown |
| Stats | `stats_block` |
| Debug | `break_on_write_name`, `change_log` (conditional on `XASH_DEBUG_CVARS`) |

The two built-in cvars (`builtin_cmd_scripting`, `builtin_cl_filterstuffcmd`) are `Cvar` members of `Impl`, not file-scope globals. This ensures they are scoped to the context, allowing tests to run with clean state and preventing any inter-test contamination.

## Lifecycle

### init()

```cpp
[[nodiscard]] bool CmdCvarContext::init(const CmdCvarInitParams &params) noexcept;
```

1. Creates the `cmd_cvar` memory pool via `memory::create_pool("cmd_cvar")`. Returns `false` (with a logged `Error`-level message) if OOM.
1. Stores the injected `trust_oracle` and `compat_policy` pointers.
1. Wires `cvar_map`, `cmd_map`, and `alias_map` to the new pool.
1. Registers the two built-in engine cvars (`cmd_scripting`, `cl_filterstuffcmd`).
1. Registers the built-in commands (`echo`, `alias`, `exec`, `wait`, `if`, `else`, `cmdlist`, `cvarlist`, `hashstats`).

`init()` is callable again after `shutdown()` — reinitialises cleanly.

### shutdown()

```cpp
void CmdCvarContext::shutdown() noexcept;
```

1. Clears both command queues (`cmd_text`, `filteredcmd_text`).
1. Calls `clear_nodes()` on all three hash maps (frees chain nodes; does not touch `V*`).
1. Destroys the `cmd_cvar` pool (frees all pool-allocated strings and structs in bulk).
1. Resets all `Impl` fields to their zero-initialised defaults (ready for a future `init()` call).

`pending_unlink` is allocated on the system heap (not the pool) so it can be safely drained and cleared after pool destruction. The command queues (`std::deque<std::string>`) use the system heap for the same reason; they are always empty at the point `destroy_pool` runs.

### Registry phases

| Phase | When | Allowed operations |
|---|---|---|
| **Pre-init** | Before `init()` | None; all methods are no-ops if `impl_` is null |
| **Init** | Inside `init()` | Register engine cvars and commands; create pool |
| **DLL load** | After `init()`, before first frame | DLL-owned cvars and commands registered via ABI shim |
| **Runtime** | Game loop frames | `cvar_set`, `cbuf_execute`, read-only queries |
| **DLL unload** | On DLL reload | `cvar_prepare_to_unlink`, DLL unload, `unlink_pending_cvars` |
| **Shutdown** | `shutdown()` call | Registry teardown; all cvar/command pointers invalid afterward |

## Threading model

The context is **game-thread-only** for all mutation (init, shutdown, register, unlink, set, cbuf_execute). The public lifecycle mutators `init`, `shutdown`, `set_server_dll_loaded`, and `set_client_dll_loaded` guard this at entry with `::xash::core::assert_thread_role(::xash::core::ThreadRole::Main)` (Q-22 / TH-Role). The remaining mutators run inside the same main-thread dispatch and are not individually asserted so the unit-test harness can drive them directly.

The following are safe to read from other threads lock-free once the context is in the **Runtime** phase:

- `Cvar::generation` — `std::atomic<uint32_t>`, monotonically increasing, `memory_order_relaxed`
- `Cvar::write_count` — `std::atomic<uint32_t>` (XASH_STATS builds only)

`FCVAR_CHANGED` in `Cvar::abi.flags` is maintained for DLL ABI compatibility but is a plain `uint32_t`, not atomic. Cross-thread polling of that bit is a data race; internal consumers should use `generation` instead.

## Error handling

- `init()` returns `bool`. All other public methods are `void` or return their result directly. No exceptions are thrown anywhere in the module.
- When `impl_` is null (constructor OOM), all public methods that dereference `impl_` silently become no-ops. This prevents crashes when the host continues running after a failed init.

## See also

- [cvar-registry.md](./cvar-registry.md)
- [command-buffer.md](./command-buffer.md)
- [dependency-injection.md](./dependency-injection.md)
