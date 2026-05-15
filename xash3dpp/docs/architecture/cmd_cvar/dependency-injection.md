# Dependency Injection

> **Defined in**: `observers.hpp`, `private/cmd_cvar/compat_policy.hpp`, `compat_goldsrc.cpp`, `compat_null.cpp`
> **Namespace**: `xash::cmd_cvar`

## Overview

The legacy engine had direct calls from the command system into server and client code (`SV_Active`, `SV_GetMaxClients`, `CL_Userinfo`, etc.), creating circular dependencies that made the modules impossible to test in isolation. The rewrite breaks every such dependency using three injected interfaces:

| Interface | Replaces | Injection mechanism |
|---|---|---|
| `ITrustOracle` | `SV_Active() && SV_GetMaxClients() == 1` | `CmdCvarInitParams::trust_oracle` |
| `ICvarObserver` | `SV_Serverinfo`, `CL_Userinfo`, `CL_UpdateInfo` direct calls | `CmdCvarContext::add_cvar_observer(observer, flag_mask)` |
| `ICompatPolicy` | `HACKS_RELATED_HLMODS` ifdef table | CMake link-time selection |

## ITrustOracle

```cpp
struct ITrustOracle {
    [[nodiscard]] virtual bool stuffcmd_is_trusted() const noexcept = 0;
};
```

Called once per `cbuf_execute()` after `cmd_text` is drained, to decide whether `filteredcmd_text` entries should run with full privilege.

The server subsystem constructs a concrete implementation that checks `SV_Active() && SV_GetMaxClients() == 1`. Tests use a stub that always returns `false`. `cmd_cvar` never imports any server header.

Injected via `CmdCvarInitParams::trust_oracle` at `init()`. The pointer is stored non-owning in `Impl`; the implementing object must outlive the context.

## ICvarObserver

```cpp
struct ICvarObserver {
    virtual void on_cvar_changed(Cvar *cvar, const char *old_value) noexcept = 0;
};
```

Registered via `CmdCvarContext::add_cvar_observer(observer, flag_mask)`. The observer is called **synchronously on the game thread** immediately after the cvar's value and `FCVAR_CHANGED` are updated. `old_value` points to the previous string value and is valid only for the duration of the call.

### flag_mask filtering

`flag_mask` is a bitwise OR of `CvarFlags` values. The observer is called only when the written cvar's `abi.flags & flag_mask != 0`. Typical registrations:

| Subsystem | flag_mask |
|---|---|
| Server | `FCVAR_SERVER \| FCVAR_MOVEVARS` |
| Client | `FCVAR_USERINFO` |
| Host | `FCVAR_VIDRESTART` |

### Observer list

The observer list is a fixed-capacity `std::array<ObserverEntry, limits::cmd_observer_max>` in `Impl`. It is populated at init time (before or immediately after `init()`) and is **never mutated during runtime**. This allows `cbuf_execute` to iterate the list without any synchronisation.

### Re-entrancy prohibition

An `ICvarObserver` implementation **must not** call `cvar_set` or any other `CmdCvarContext` mutation method from within `on_cvar_changed`. Such a call would re-enter the write path while it is mid-update, potentially corrupting the observer notification loop or the change log.

## ICompatPolicy

```cpp
struct ICompatPolicy {
    [[nodiscard]] virtual const char *redirect_cvar_name(const char *name) const noexcept = 0;
    [[nodiscard]] virtual bool is_filterable_exempt(const char *cmd_name) const noexcept = 0;
    [[nodiscard]] virtual bool is_overridable_command(const char *cmd_name) const noexcept = 0;
};
```

Unlike the other two interfaces, `ICompatPolicy` is **not injected at runtime** — it is selected at CMake link time:

- `XASH_GOLDSRC_COMPAT=1` → links `compat_goldsrc.cpp` → `GoldSrcCompatPolicy`
- `XASH_GOLDSRC_COMPAT=0` → links `compat_null.cpp` → `NullCompatPolicy`

This means zero `#ifdef` in any core source file. The compat implementation is a `constexpr` static table in `compat_goldsrc.cpp`; changing it is isolated from all other subsystem code.

`ICompatPolicy` is declared in `private/cmd_cvar/compat_policy.hpp`. This header is **not part of the public API** — it is included only by `context_init.cpp` (which constructs the policy), `compat_goldsrc.cpp`, and `compat_null.cpp`.

### GoldSrc compatibility quirks

| Method | Quirk description |
|---|---|
| `redirect_cvar_name` | Maps certain HL25 cvar names to their canonical equivalents (e.g. `gl_widescreen_yfov` → `r_adjust_fov`) |
| `is_filterable_exempt` | Returns `true` for specific commands in `ricochet` and `dod` that must bypass `cl_filterstuffcmd` |
| `is_overridable_command` | Returns `true` for the fixed set of engine commands that game DLLs are permitted to silently replace |

`NullCompatPolicy` returns `nullptr` / `false` for all three methods.

## CmdCvarInitParams

All injected dependencies are bundled into a single named-parameters struct:

```cpp
struct CmdCvarInitParams {
    ITrustOracle  *trust_oracle   = nullptr;  // required
    ICompatPolicy *compat_policy  = nullptr;  // required; typically global singleton
    const char    *cmdline        = nullptr;  // +arg parsing; may be nullptr
};
```

Passing `nullptr` for `trust_oracle` disables privilege escalation (stuffcmd always runs unprivileged). This is the correct setting for tests.

## Testing without the server

Because all three dependencies are injected, `test_cmd_cvar` constructs a `CmdCvarContext` with minimal stubs:

- `TrustOracleStub` — always returns `false`
- `NullCompatPolicy` — no redirects, no exemptions
- No `ICvarObserver` registered

This gives complete coverage of the command and cvar subsystems without linking the server, client, or host modules.

## See also

- [command-buffer.md](./command-buffer.md) — where `ITrustOracle` and `ICompatPolicy` are consumed
- [cvar-registry.md](./cvar-registry.md) — where `ICvarObserver` is triggered
- Boundary doc: D2 (trust oracle), D3 (change observers), D12 (compat isolation)
