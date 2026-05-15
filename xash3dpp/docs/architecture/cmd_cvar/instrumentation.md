# Instrumentation

> **Defined in**: `cvar.hpp`, `context.hpp`, `private/cmd_cvar/circular_buffer.hpp`, `context_misc.cpp`
> **Namespace**: `xash::cmd_cvar`

## Overview

`cmd_cvar` implements the engine-wide three-tier instrumentation model:

| Tier | Build guard | Per-write cost |
|---|---|---|
| Always-on | none | One `relaxed` `fetch_add` on `generation` |
| Stats | `#if XASH_STATS` | Compile-time elided to nothing in release |
| Debug | `#if XASH_DEBUG_CVARS` | Compile-time elided to nothing in release |

Release builds pay only for `Cvar::generation` — one atomic increment per write.

## Always-on: generation counter

Every `Cvar` carries:

```cpp
std::atomic<uint32_t> generation { 0 };
```

Incremented on every write via `memory_order_relaxed`. Consumers (renderer, physics, sound) that need efficient change detection store the last-seen generation and compare, avoiding `FCVAR_CHANGED` polling or any lock.

`FCVAR_CHANGED` is still set in `abi.flags` on every write for legacy DLLs that poll it — that bit is maintained purely for DLL ABI compatibility.

## XASH_STATS tier

### CmdCvarStats

```cpp
struct CmdCvarStats {
    std::atomic<uint64_t> commands_executed { 0 };  // total commands dispatched
    std::atomic<uint64_t> commands_dropped  { 0 };  // commands dropped by privilege filter
    std::atomic<uint32_t> buffer_high_water { 0 };  // peak cmd_text queue depth (entries)
    uint32_t              peak_cvar_count   { 0 };  // maximum cvars registered simultaneously
    uint32_t              peak_command_count{ 0 };  // maximum commands registered simultaneously
};
```

Exposed read-only via `CmdCvarContext::stats() const noexcept`. The returned reference is valid for the context's lifetime. `commands_executed` and `commands_dropped` are atomic; `peak_*` fields are game-thread-only.

### Per-cvar stats

When `XASH_STATS` is defined, three fields are added to `Cvar`:

| Field | Type | Updated by |
|---|---|---|
| `write_count` | `std::atomic<uint32_t>` | Every `cvar_set`; safe to read from any thread |
| `last_write_frame` | `uint32_t` | Every `cvar_set`; game-thread-only |
| `last_write_source` | `CvarWriteSource` | Every `cvar_set`; game-thread-only |

### CvarWriteSource

```cpp
enum class CvarWriteSource : uint8_t {
    Init,            // initial value at registration
    Console,         // typed at the local console
    ExecConfig,      // sourced from an exec'd .cfg file
    StuffCmd,        // arrived via server stuffcmd
    EngineInternal,  // engine code (e.g. cvar_set_cheat_state)
    GameDLL,         // pfnCvar_DirectSet / pfnCvar_RegisterVariable
    ClientDLL,       // pfnRegisterVariable from the client DLL
    MenuDLL,         // pfnRegisterVariable from the menu DLL
    Script,          // set by an external scripting engine
};
```

`CvarWriteSource::Init` (capital I) is the PascalCase-compliant QF name. Stored in `Cvar::last_write_source` in XASH_STATS builds; also used in `CvarChangeRecord`.

### Per-command stats

When `XASH_STATS` is defined, `Command` carries:

```cpp
std::atomic<uint64_t> execute_count { 0 };
```

Incremented on every dispatch. Allows a profiler to identify the most-invoked commands per frame without any additional instrumentation.

## XASH_DEBUG_CVARS tier

### Change log

`Impl` holds:

```cpp
CircularBuffer<CvarChangeRecord, limits::cvar_change_log_capacity> change_log;
```

Every `cvar_set` appends a `CvarChangeRecord`:

```cpp
struct CvarChangeRecord {
    const char     *cvar_name;   // stable pointer into registry; valid while context is alive
    char            old_value[64];
    char            new_value[64];
    uint32_t        frame;
    CvarWriteSource source;
};
```

`cvar_change_log_capacity` defaults to 256 (defined in `limits.hpp`, CMake-overridable). When the buffer is full, the oldest entry is overwritten (ring semantics). The change log is accessible via the `cvar_changelog` built-in command (registered only in debug builds).

### Break-on-write

```cpp
void CmdCvarContext::debug_break_on_cvar_write(const char *cvar_name) noexcept;
```

Stores `cvar_name` in `Impl::break_on_write_name`. Every subsequent `cvar_set` checks the name and calls `XASH_DEBUG_BREAK()` if it matches, pausing the process in the debugger. Pass `nullptr` to clear. This is the primary tool for tracking down unexpected cvar mutations in large mods.

## CircularBuffer

`detail::CircularBuffer<T, N>` is a private fixed-capacity ring template used only by the change log. It is not exported or included outside the `cmd_cvar` source tree.

Interface:

```cpp
template<typename T, std::size_t N>
class CircularBuffer {
    void push(const T &value);            // overwrites oldest when full
    const T &operator[](std::size_t i);  // logical index: 0=oldest, size()-1=newest
    std::size_t size() const;
    bool empty() const;
    Iterator begin();
    Iterator end();
};
```

Not thread-safe; written only on the game thread during `cvar_set`.

## dump_hash_stats()

```cpp
void CmdCvarContext::dump_hash_stats() const noexcept;
```

Prints bucket-fill distribution for `cvar_map`, `cmd_map`, and `alias_map` to the platform console. Available in all build tiers. Use to tune `limits::cvar_hash_buckets` for mods with large numbers of cvars.

## See also

- [cvar-registry.md](./cvar-registry.md) — `Cvar` struct, stats fields in context
- [context-and-lifecycle.md](./context-and-lifecycle.md) — `CmdCvarStats` ownership
- `xash3dpp/docs/design/debug-stats-design.md` — engine-wide three-tier instrumentation model
