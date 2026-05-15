# Command Buffer and Dispatch

> **Defined in**: `context.hpp`, `command.hpp`, `cmd_dispatch.cpp`, `cmd_ops.cpp`, `base_cmd.cpp`
> **Namespace**: `xash::cmd_cvar`

## Overview

The command buffer accepts text from multiple sources, queues it in two privilege-separated deques, and dispatches one frame's worth per `cbuf_execute()` call. Commands are dispatched to registered `CommandFn` callbacks or to cvar-set handlers.

## Dual-queue privilege model

Two `std::deque<std::string>` queues segregate commands by trust level:

| Queue | `Impl` member | Source | Privilege |
|---|---|---|---|
| **cmd_text** | `Impl::cmd_text` | Local console, `exec`, `cbuf_insert_text`, `+args` | Always trusted |
| **filteredcmd_text** | `Impl::filteredcmd_text` | Server `stuffcmd`, `cbuf_stuff_text` | Trusted only when `ITrustOracle::stuffcmd_is_trusted()` returns `true` |

`ITrustOracle` encapsulates the legacy `SV_Active() && SV_GetMaxClients() == 1` check. `cmd_cvar` never imports server headers.

### cl_filterstuffcmd

When the `cl_filterstuffcmd` built-in cvar is non-zero, commands and cvar-sets arriving from the stuffcmd queue that match well-known client-side prefixes (`r_`, `cl_`, `gl_`, `m_`, etc.) or carry `FCVAR_FILTERABLE` are dropped before dispatch. The `ICompatPolicy::is_filterable_exempt()` method provides the GoldSrc HL-mod override table for known exemptions.

## cbuf_execute

```cpp
void CmdCvarContext::cbuf_execute() noexcept;
```

1. If `cmd_wait > 0`: decrement and return (skip this frame's dispatch).
2. Dispatch commands from `cmd_text` (privileged) until the queue is drained.
3. When `cmd_text` is empty, check `ITrustOracle::stuffcmd_is_trusted()`.
   - If **trusted**: promote all `filteredcmd_text` entries to `cmd_text` and dispatch them with full privilege.
   - If **untrusted**: dispatch `filteredcmd_text` entries through the filter check.
4. Each command line is tokenised and routed by `cmd_dispatch_line()`.

`cbuf_execute` must not be called re-entrantly. A `CommandFn` that calls `cbuf_execute()` again will overwrite the tokenizer scratch state in `Impl`.

## Tokenizer

The tokenizer runs inside `cbuf_execute` before dispatching each command. Results are stored in `Impl::tok_argc`, `tok_argv`, `tok_argsBuffer`, and `tok_is_privileged` — scratch state that is valid only for the duration of the current `CommandFn` call.

```cpp
[[nodiscard]] int         CmdCvarContext::cmd_argc() const noexcept;
[[nodiscard]] const char *CmdCvarContext::cmd_argv(int i) const noexcept;
[[nodiscard]] const char *CmdCvarContext::cmd_args() const noexcept;
[[nodiscard]] bool        CmdCvarContext::cmd_current_is_privileged() const noexcept;
```

Calling these accessors outside a `CommandFn` dispatch returns `0`, `""`, or `false`.

A thread-local `tls_ctx` pointer (defined in `context.cpp`) is set to point at `this` while dispatch is running. Built-in commands access the tokenizer via the thread-local without needing an explicit context parameter — matching the legacy GoldSrc API where `Cmd_Argc()` etc. were global free functions.

## cmd_scripting and variable substitution

When the `cmd_scripting` built-in cvar is non-zero **and** the command arrived from a privileged source, the tokenizer expands `$cvar_name` tokens to the current string value of the named cvar before dispatching. `cmd_scripting` carries `FCVAR_PRIVILEGED`, so a server cannot enable it via `stuffcmd`.

The `if` / `else` / `endif` built-in commands use `Impl::condlevel` and `Impl::cmd_condition` to manage nesting. Nesting depth is bounded by `limits::cmd_condition_depth`.

## Command registry

| Operation | Method |
|---|---|
| Register | `cmd_add(name, fn, flags, desc)` |
| Remove | `cmd_remove(name)` |
| Bulk remove by flag | `cmd_unlink(flags_mask)` |
| Lookup | `cmd_exists(name) -> bool` |
| Describe | `cmd_describe(name) -> CommandDesc` |
| Execute immediately | `cmd_execute_string(text)` |

Commands are stored in `CmdHashMap<Command>` for O(1) lookup and a linked list through `Command::next` for `cmdlist` enumeration.

`FCMD_OVERRIDABLE`: if a game DLL registers a command with the same name as an existing engine command that carries `FCMD_OVERRIDABLE`, the DLL's handler silently replaces the engine's. The original handler is saved and restored on `cmd_unlink(FCMD_EXTDLL)`.

## CommandFlags

| Flag | Value | Meaning |
|---|---|---|
| `FCMD_EXTDLL` | `1 << 0` | Registered by the server DLL |
| `FCMD_CLIENTDLL` | `1 << 1` | Registered by the client DLL |
| `FCMD_GAMEUIDLL` | `1 << 2` | Registered by the menu DLL |
| `FCMD_PRIVILEGED` | `1 << 3` | Executes only from a trusted source |
| `FCMD_OVERRIDABLE` | `1 << 4` | Game DLL may silently replace this command |

## Built-in commands

Registered in `base_cmd.cpp` during `init()`:

| Command | Behaviour |
|---|---|
| `echo` | Writes arguments to the platform console |
| `alias` | Defines a command alias (expands to stored text on invocation) |
| `exec` | Loads and appends a `.cfg` file to `cmd_text` |
| `wait` | Increments `cmd_wait` by 1 (defers next frame's dispatch) |
| `if` / `else` / `endif` | Conditional scripting blocks (requires `cmd_scripting` non-zero) |
| `cmdlist` | Dumps all registered commands to the console |
| `cvarlist` | Dumps all registered cvars to the console |
| `hashstats` | Prints bucket-fill distribution for `cvar_map`, `cmd_map`, `alias_map` |

`hashstats` is registered in all build tiers; the output is only meaningful when debugging hash-map performance.

## cbuf_add_text vs. cbuf_insert_text vs. cbuf_stuff_text

| Method | Queue | Position |
|---|---|---|
| `cbuf_add_text(text)` | `cmd_text` | Back (appended) |
| `cbuf_insert_text(text)` | `cmd_text` | Front (prepended) |
| `cbuf_stuff_text(text)` | `filteredcmd_text` | Back |

`cbuf_insert_text` is used by `exec` to run a config file before other queued text, preserving the legacy `Cbuf_InsertText` behaviour.

## Threading model

`cbuf_execute` and all registry mutations are **game-thread-only**. The tokenizer scratch state is not protected by any lock — a `CommandFn` that triggers a re-entrant `cbuf_execute` would corrupt it.

## See also

- [cvar-registry.md](./cvar-registry.md) — where `cvar_set` fits into the write path
- [dependency-injection.md](./dependency-injection.md) — `ITrustOracle` and `ICompatPolicy`
- Legacy: `engine/common/cmd.c`, `engine/common/base_cmd.c`
