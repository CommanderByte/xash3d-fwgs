# cmd_cvar — Architecture Overview

> **Source**: `xash3dpp/src/cmd_cvar/`
> **Public API**: `xash3dpp/include/xash3dpp/cmd_cvar/`
> **Legacy reference**: `engine/common/cmd.c`, `engine/common/cvar.c`, `engine/common/base_cmd.c`

## Purpose

`cmd_cvar` owns the **command buffer**, **command registry**, **console-variable (cvar) registry**, and the text-scripting layer that connects them. It accepts text input from any source (local console, config files, server `stuffcmd`, command-line `+args`), tokenises it, and dispatches it to registered command functions or cvar write handlers. It manages the full lifecycle of commands and cvars from registration through DLL unload.

It does **not** own: console display or input bindings, network packet encoding/decoding, filesystem I/O beyond reading `.cfg` files, or the host's game loop.

## Design goals

- **ABI-stable cvar layout**: `Cvar` begins with `CvarAbi`, which is layout-identical to the legacy `cvar_t`. Legacy game DLLs receive `cvar_t *` pointers and dereference them directly; the struct layout must never change.
- **Circular-dependency elimination**: The legacy code called server and client functions directly from inside `Cbuf_Execute` and `Cvar_UpdateInfo`. The rewrite breaks this with three injected interfaces (`ITrustOracle`, `ICvarObserver`, `ICompatPolicy`).
- **Zero `#ifdef` in core files**: GoldSrc compatibility quirks live entirely in `compat_goldsrc.cpp` behind `ICompatPolicy`. The `XASH_GOLDSRC_COMPAT` CMake option selects the implementation at link time.
- **Lock-free change detection**: Every `Cvar` carries a `std::atomic<uint32_t> generation` counter incremented on each write, allowing consumers to detect changes without polling `FCVAR_CHANGED` or holding a lock.
- **Privileged vs. unprivileged queues**: Two separate `std::deque<std::string>` buffers isolate engine-trusted commands from server-sent `stuffcmd` text. Privilege escalation requires an `ITrustOracle` decision, not a baked-in server-state check.
- **Three-tier instrumentation**: Release builds pay nothing extra; `XASH_STATS` adds lightweight atomics; `XASH_DEBUG_CVARS` adds the change log and break-on-write facility.

## Key invariants

- `Cvar::abi` must be at offset 0 in `Cvar` at all times (enforced by `static_assert`).
- Cvars are **never moved or reallocated** after registration — DLLs hold raw `cvar_t *` pointers that must remain stable for the process lifetime.
- All registry mutations (register, unlink, set) are **game-thread-only**. The `generation` atomic allows consumers on other threads to detect changes without mutating state.
- `cbuf_execute()` must not be called re-entrantly. A `CommandFn` that calls `cbuf_execute()` again will corrupt the tokenizer scratch state.
- The observer list is **frozen after init** — `add_cvar_observer` must be called before or immediately after `init()`, never during runtime.
- `cvar_prepare_to_unlink()` must be called **before** the DLL is unloaded (while cvar structs are still valid); `unlink_pending_cvars()` is called after.

## Relationship to legacy code

The legacy engine's `Cmd_*` and `Cvar_*` functions were free functions operating on file-scope globals. The rewrite moves all state into `CmdCvarContext` (a pimpl class), eliminating file-scope mutable globals. A thin ABI shim in the host layer re-exports the legacy function signatures using a single `g_cmd_cvar` context pointer that tests never touch.

Direct calls to `SV_Active()`, `SV_GetMaxClients()`, `CL_Userinfo()`, etc., from inside the command system are replaced by the `ITrustOracle` and `ICvarObserver` injection interfaces.

The fixed 32 KB ring buffer for the command text is replaced by `std::deque<std::string>` — no fixed upper bound, O(1) append and prepend, no `memmove` on `cbuf_insert_text`.

## Architecture at a glance

```
          [host layer / game loop]
                    |
                    | init(CmdCvarInitParams)
                    v
         +---------------------+
         |   CmdCvarContext    |  ----> engine subsystems (via public API)
         |   (pimpl class)     |
         +--------+------------+
                  | impl_
                  v
    +------------------------------------------+
    |                 Impl                      |
    |                                          |
    |  cmd_text ----> cbuf_execute --> dispatch |
    |  filteredcmd_text    |              |     |
    |                      |  ITrustOracle|     |
    |  cvar_map <--------- cvar_set   ICvarObs  |
    |  cvar_list_head                           |
    |  cmd_map <--------------------- cmd_add   |
    |  alias_map                                |
    |                                          |
    |  ICompatPolicy ------ compat quirks       |
    +------------------------------------------+
```

## Index of concepts

- [index.md](./index.md) — full file/symbol index
- [context-and-lifecycle.md](./context-and-lifecycle.md) — `CmdCvarContext`, pimpl body, init/shutdown, registry phases
- [cvar-registry.md](./cvar-registry.md) — `Cvar`, `CvarAbi`, flags, registration paths, ABI stability
- [command-buffer.md](./command-buffer.md) — dual queues, privilege model, tokenizer, dispatch, scripting
- [dependency-injection.md](./dependency-injection.md) — `ITrustOracle`, `ICvarObserver`, `ICompatPolicy`, GoldSrc compat layer
- [instrumentation.md](./instrumentation.md) — stats tiers, `CmdCvarStats`, `CvarWriteSource`, change log, break-on-write
