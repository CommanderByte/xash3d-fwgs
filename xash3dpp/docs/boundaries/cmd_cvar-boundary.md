# cmd_cvar Boundary Spec

## Responsibility

This module owns the command buffer, command registry, console-variable (cvar)
registry, and the text-scripting layer that binds them together. It accepts
text commands from any source (console, config files, server stuffcmds,
command-line `+` args), tokenises them, dispatches them to registered command
functions or cvar set-handlers, and manages the lifetime of both commands and
cvars. It does **not** own the console display, input bindings, networking
encode/decode, or filesystem I/O beyond reading `.cfg` files.

## External ABI contracts

### Game DLL (`engine/eiface.h`) — FROZEN

| Callback | Signature | Notes |
|---|---|---|
| `pfnCvar_RegisterVariable` | `void (cvar_t *variable)` | DLL passes a static `cvar_t`; engine copies `.string` into its pool and inserts the pointer into the registry. The original pointer must remain stable — DLL caches it. |
| `pfnCVarGetPointer` | `cvar_t *(const char *name)` | Returns a **live** pointer into the registry; DLL dereferences `.string` and `.value` directly. Layout of `cvar_t` is frozen. |
| `pfnCvar_DirectSet` | `void (cvar_s *var, const char *value)` | Bypasses privilege check; used by game DLL to force a value. |
| `pfnAddServerCommand` | `void (const char *name, void (*fn)(void))` | Registers a command that will be tagged `CMD_SERVERDLL` and unlinked when the game DLL unloads. |

### Client DLL (`engine/cdll_int.h`) — FROZEN

| Callback | Signature | Notes |
|---|---|---|
| `pfnRegisterVariable` | `cvar_t *(const char *name, const char *value, int flags)` | Same ownership semantics as `pfnCvar_RegisterVariable`. |
| `pfnGetCvarFloat` | `float (const char *name)` | |
| `pfnGetCvarString` | `const char *(const char *name)` | |
| `pfnAddCommand` | `int (const char *name, void (*fn)(void))` | Tagged `CMD_CLIENTDLL`. |
| `pfnClientCmd` | `int (const char *text)` | Appends to the privileged command buffer. |
| `pfnServerCmd` | `int (const char *text)` | Sends text as `clc_stringcmd` to the server. |

### Menu DLL (`engine/menu_int.h`) — FROZEN

| Callback | Signature |
|---|---|
| `pfnRegisterVariable` | `cvar_t *(const char *name, const char *value, int flags)` |

### Shared structure layout — FROZEN

`cvar_t` from `common/cvardef.h`:

```c
typedef struct cvar_s {
    const char *name;
    const char *string;
    int         flags;   // FCVAR_* bitmask (bits 0-23 public; 16-29 engine-only)
    float       value;
    struct cvar_s *next; // linked-list chain exposed to DLLs via Cvar_GetList
} cvar_t;
```

Game and client DLLs read `.string`, `.value`, `.flags`, and `.next` directly.
Any change to this layout breaks the frozen ABI.

## Interface (what the rest of the engine calls)

### Command buffer

| Function | Purpose |
|---|---|
| `Cbuf_AddText(text)` | Append to the privileged (engine-side) command queue |
| `Cbuf_AddFilteredText(text)` | Append to the unprivileged (stuffcmd) queue |
| `Cbuf_InsertText(text)` | Prepend to the privileged queue (for `exec`) |
| `Cbuf_Execute()` | Drain both queues, executing one frame's worth |
| `Cbuf_Clear()` | Reset both queues to empty |

### Command tokenizer (valid only during execution of a command)

| Function | Purpose |
|---|---|
| `Cmd_Argc()` | Number of tokens |
| `Cmd_Argv(i)` | Token by index |
| `Cmd_Args()` | Everything after token 0 as a raw string |
| `Cmd_CurrentCommandIsPrivileged()` | Whether the executing command came from the privileged buffer |

### Command registry

| Function | Purpose |
|---|---|
| `Cmd_AddCommand(name, fn, desc)` | Register a zero-argument command function |
| `Cmd_RemoveCommand(name)` | Unregister |
| `Cmd_Exists(name)` | Lookup |
| `Cmd_Unlink(group)` | Bulk-remove by flag group (DLL unload) |
| `Cmd_ExecuteString(text)` | Execute a single command line immediately (privileged) |

### Cvar registry

| Function | Purpose |
|---|---|
| `Cvar_Get(name, default, flags, desc)` | Get-or-create a cvar |
| `Cvar_RegisterVariable(convar_t *)` | Register a statically-defined cvar |
| `Cvar_FindVar(name)` | Lookup by name |
| `Cvar_Set(name, value)` | Set by name (respects privilege/cheat/latch) |
| `Cvar_DirectSet(var, value)` | Set by pointer (bypasses name lookup) |
| `Cvar_FullSet(name, value, flags)` | Force-set, bypasses all guards |
| `Cvar_VariableString/Value/Integer(name)` | Read helpers |
| `Cvar_Unlink(group)` | Bulk-remove by flag group (DLL unload) |
| `Cvar_PrepareToUnlink / UnlinkPendingCvars` | Safe unlink when DLL may have freed the struct |
| `Cvar_WriteVariables(file, group)` | Persist to `.cfg` |
| `Cvar_SetCheatState()` | Reset all `FCVAR_CHEAT` cvars to defaults |
| `Cvar_GetList()` | Returns `cvar_t *` head of list (exposed to DLLs) |

## Dependencies (what this module calls)

| Subsystem | Why |
|---|---|
| **memory** | `Mem_Malloc/Realloc/Free`, pool handles for `cmd_pool`, `cvar_pool`, `basecmd_pool` |
| **filesystem** | `FS_LoadFile` (exec), `FS_Search` (wildcard exec), `FS_Printf` (write vars) |
| **platform** | `Con_Printf/DPrintf/Reportf` (output), `Host_Error` (fatal overflow) |
| **server** *(indirect)* | `SV_Active()`, `SV_GetMaxClients()` — used inside `Cbuf_Execute` to decide privilege level of the stuffcmd buffer; also `SV_Serverinfo`, `SV_BroadcastCommand/Printf` called from `Cvar_UpdateInfo` when an `FCVAR_SERVER` cvar changes |
| **client** *(indirect)* | `CL_Userinfo()`, `CL_UpdateInfo()` — called from `Cvar_UpdateInfo` when an `FCVAR_USERINFO` cvar changes; `cls.state`, `cls.netchan.message`, `MSG_*` — used in `Cmd_ForwardToServer` |

The server and client dependencies are the **circular-dependency landmine**.
In the rewrite these must be replaced with an observer/callback interface
injected at init time; the `cmd_cvar` module must not `#include` server or
client headers.

## Owned state

### Context-level state

| Name | Type | Purpose |
|---|---|---|
| `cmd_text` | `std::deque<std::string>` | Privileged command queue; informally bounded by `limits::cbuf_size` (default 256 entries) |
| `filteredcmd_text` | `std::deque<std::string>` | Unprivileged (stuffcmd) command queue; same bound as `cmd_text` |
| `cmd_wait` | `int` | Frame-delay counter for the `wait` command |
| `cmd_functions` | hash map + linked ABI chain | Registered command functions |
| `cmd_alias` | hash map + linked ABI chain | Command aliases |
| `cvar_registry` | hash map | Primary lookup structure; also maintains `next` ABI chain for `Cvar_GetList` |
| `tokeniser_state` | scratch (argc/argv/args) | Valid only during execution of one command; game-thread-only |
| `cmd_condition/condlevel` | `uint32_t / int` | State for `if`/`else` scripting |
| `current_is_privileged` | `bool` | Runtime privilege tag for the executing command |
| `trust_oracle` | `ITrustOracle *` | Injected at init; answers stuffcmd trust question |
| `compat_policy` | `ICompatPolicy *` | Injected at init; routes compat quirks |
| `observers` | `InlineVector<{mask, ICvarObserver*}>` | Registered change observers; fixed at init |
| `stats` | `CmdCvarStats` | Lifetime counters (see instrumentation section) |

### Per-`Cvar` fields

| Field | Always present | `XASH_STATS` | `XASH_DEBUG_CVARS` |
|---|---|---|---|
| `name`, `string`, `flags`, `value`, `next` | ✓ ABI-frozen | | |
| `def_string`, `desc` | ✓ engine extension | | |
| `generation` (atomic) | ✓ | | |
| `type_hint`, `range_min`, `range_max` | ✓ | | |
| `owner_flags` | ✓ | | |
| `write_count` (atomic), `last_write_frame`, `last_write_source` | | ✓ | |
| `last_write_location` | | | ✓ |

### Per-`Command` fields

| Field | Always present | `XASH_STATS` |
|---|---|---|
| `name`, `function`, `flags`, `desc` | ✓ | |
| `execute_count` (atomic) | | ✓ |

### Context-level debug state (`XASH_DEBUG_CVARS` only)

| Name | Purpose |
|---|---|
| `change_log` | Circular buffer of last `limits::cvar_change_log_capacity` changes |
| `break_on_write_name` | Triggers `XASH_DEBUG_BREAK()` when the named cvar is written |

## Quirks and invariants

- **`cvar_t` pointer stability**: `pfnCVarGetPointer` returns a pointer game DLLs hold for the engine lifetime. Cvars must never be moved in memory. The internal registry cannot use a relocating container (no `std::vector<convar_t>`); use a stable-address allocator or intrusive list.

- **`cvar_t` vs `convar_t` sentinel**: Engine-defined cvars use a longer `convar_t` struct with `def_string`, `desc`, and `next` fields. DLL-registered cvars come in as the short `cvar_t`. The legacy code detects the difference via `CVAR_CHECK_SENTINEL` — a magic value placed in a specific field offset. The rewrite needs a clean solution (e.g., separate registration path, or a flag bit).

- **Two-queue privilege model**: `cmd_text` is always trusted (local console, `exec`). `filteredcmd_text` carries server-sent `stuffcmd` text and is only executed as privileged when `SV_Active() && SV_GetMaxClients() == 1` (singleplayer). In multiplayer, stuffed commands go through `Cmd_ShouldAllowCommand` / `Cvar_ShouldSetCvar`.

- **`cl_filterstuffcmd`** (`FCVAR_ARCHIVE|FCVAR_PRIVILEGED`): When > 0, commands and cvars matching well-known client prefixes (`r_`, `cl_`, `gl_`, `m_`, `hud_`, `joy_`, `con_`, `scr_`) or tagged `FCVAR_FILTERABLE` are blocked from unprivileged execution. There is also a game-specific quirk table (`#ifdef HACKS_RELATED_HLMODS`) that exempts specific cvars for `ricochet` and `dod`.

- **`cmd_scripting`** (`FCVAR_PRIVILEGED`): When enabled, `$cvar_name` in a command string is replaced with the cvar's current string value before tokenisation, and `if`/`else` conditional blocks are supported. This feature is privileged to prevent servers from using it.

- **`FCVAR_CHANGED` polling**: The `FCVAR_CHANGED` bit is set on every cvar write and cleared by consumers (renderer, physics, etc.) that poll it. This is a bitmask side-channel, not a proper callback. The rewrite should replace it with typed change-observer hooks, but must keep the bit in the ABI struct for legacy DLL compat.

- **`Cvar_Unlink` / `Cvar_PrepareToUnlink`**: Game DLLs sometimes free cvar struct memory without telling the engine. `pending_cvar_t` captures the necessary metadata (name, next pointer) before unload so the registry can be cleaned up safely. The rewrite must handle the same scenario.

- **Auto-create on `set`**: `Cvar_Set2` creates a cvar with `FCVAR_USER_CREATED` if the name is not found. This is intentional and relied upon by config files that reference cvars before the owning DLL is loaded.

- **`CMD_OVERRIDABLE`**: A small set of engine commands can be silently replaced by game DLL commands of the same name; the DLL's version takes over without error. The registry must support this flag.

- **HL25 compatibility**: `Cvar_FindVar("gl_widescreen_yfov")` silently redirects to `r_adjust_fov`. This quirk must be preserved.

- **`FCVAR_EXTDLL`/`FCVAR_CLIENTDLL` unlink guard**: `Cmd_Unlink` and `Cvar_Unlink` refuse to unlink if the corresponding `host_gameloaded` / `host_clientloaded` cvar is set, preventing double-unlink during DLL reload.

- **Command `wait` works in frame units**: `cmd_wait` decrements once per `Cbuf_Execute` call. One `wait` skips exactly one engine frame.

- **`Cbuf_ExecStuffCmds`** runs once: processes `+command` launch-line arguments, then unregisters the `stuffcmds` command. Calling it a second time is a no-op.

- **Config write format**: `variable "value"\n` per line. The reader (`Cmd_ExecScript`) re-tokenises these lines through the normal command path, so the format is implicitly defined by the tokeniser rules.

## Architectural decisions

These decisions were resolved during design review and supersede the original open questions.

| ID | Decision |
|----|---------|
| D1 | **Context object + global ABI shim.** All state in `CmdCvarContext`; no file-scope globals in the implementation. A single `CmdCvarContext *g_cmd_cvar` is set at host init and used only by the ABI shim free functions. Tests construct a local `CmdCvarContext` and never touch the global. |
| D2 | **Trust oracle, injected at init.** The two-queue privilege system is preserved. Whether the stuffcmd queue is trusted is answered by a `ITrustOracle` virtual interface injected at construction — `cmd_cvar` never imports server or client headers. Default: trusted when the running server is local and singleplayer. |
| D3 | **Change side-effects via `ICvarObserver`, injected observers.** Server registers for `FCVAR_SERVER`; client for `FCVAR_USERINFO`; host for `FCVAR_MOVEVARS` / `FCVAR_VIDRESTART`. Dependency arrow goes outward. `FCVAR_CHANGED` bit is still set in the ABI struct for legacy DLLs that poll it; the observer system is the clean internal mechanism. |
| D4 | **ABI-literal struct layout, two explicit DLL registration paths, safe internal extensions.** `Cvar` starts with the frozen `cvar_t` fields (offset-stable, never modified). Extensions after `next`: `generation` (atomic, lock-free change detection), `type_hint` (scripting/UI), `range_min/max` (validation), `owner_flags` (stable ownership for unlink). Two registration paths: `register_engine_cvar(Cvar &)` and `register_dll_cvar(cvar_t *)`. `next` pointer maintained as ABI list chain only; internal lookup uses a hash map. |
| D5 | **`std::deque<std::string>` command buffer.** Replaces the fixed 32 KB ring. Append and prepend are O(1) amortised; `wait` is trivial; no fixed upper bound; no memmove on insert. |
| D6 | **Compile-time limits in `limits.hpp`, CMake-overridable.** All buffer/count constants (`cmd_line_max`, `cmd_tokens_max`, `alias_name_max`, `cvar_hash_buckets`, `cbuf_size`, and later cross-subsystem constants) live in `include/xash3dpp/limits.hpp` as `inline constexpr`. A CMake option generates an override header for specialised builds. |
| D7 | **Lifecycle phases: init → DLL-load → runtime → DLL-unload.** Registry structure is frozen during runtime. Iterators are not valid across phase transitions. `Cvar_Unlink` safe even if DLL freed its structs (via saved metadata). |
| D8 | **Reads lock-free after init; writes game-thread-only.** `value` stored as `std::atomic<float>`; string values in immutable-on-write pool (pointer atomically updated on write); `FCVAR_CHANGED` bit in `std::atomic<uint32_t>` flags field. Registration and unlink: game thread only. |
| D9 | **`cmd_scripting` preserved, stays privileged.** `$cvar_name` substitution and `if`/`else` conditionals kept with identical semantics. Privilege guard blocks server-side information extraction via stuffcmd. Future scripting backends are additive; this feature remains as a baseline. |
| D10 | **Queryable and observable registry.** Each command exposes a `CommandDesc` (name, description, flags, future-ready `ParamSpec` span). Each cvar exposes a `CvarDesc` (name, default, description, flags, type hint, range). External scripting layers iterate at startup; change observers give reactive access without polling. |
| D11 | **Observer interfaces via pure virtual (`ICvarObserver`, `ITrustOracle`).** Compatible with `-fno-rtti` / `-fno-exceptions`. No heap allocation at dispatch time. Observer list is a fixed-capacity `InlineVector` registered at init, never mutated during runtime. |
| D12 | **Compat quirks isolated to `compat_goldsrc.cpp` behind `ICompatPolicy`.** Core files contain zero `#ifdef HACKS_RELATED_HLMODS`. `XASH_GOLDSRC_COMPAT` CMake option selects the real TU or a null stub at link time. The quirk table is a `constexpr` static array; each entry is a deliberate, reviewed exception. |
| D13 | **Statistics and debug instrumentation, gated by build flags.** See the *Statistics and debug instrumentation* section below. |

## Statistics and debug instrumentation

All instrumentation is gated by two compile-time flags so release builds pay nothing:

- `XASH_STATS` — lightweight counters safe to leave on in profiling/beta builds
- `XASH_DEBUG_CVARS` — heavier tracing (change log, break-on-write) for developer builds only

### `CvarWriteSource` — who last touched a cvar

```cpp
enum class CvarWriteSource : uint8_t {
    Init,           // initial value at registration
    Console,        // typed at local console
    ExecConfig,     // sourced from an exec'd .cfg file
    StuffCmd,       // arrived via server stuffcmd
    EngineInternal, // engine code (e.g. Cvar_SetCheatState)
    GameDLL,        // pfnCvar_DirectSet / pfnCvar_RegisterVariable
    ClientDLL,      // pfnRegisterVariable from client DLL
    MenuDLL,        // pfnRegisterVariable from menu DLL
    Script,         // set by an external scripting engine
};
```

This enum is present in all builds; the fields that store it are conditional (see below).

### Per-cvar fields (in `Cvar`, after the safe-extension fields)

```cpp
// XASH_STATS — present in stats and debug builds
std::atomic<uint32_t>  write_count{ 0 };     // total lifetime writes
uint32_t               last_write_frame{ 0 };// game frame of last write
CvarWriteSource        last_write_source{ CvarWriteSource::Init };

// XASH_DEBUG_CVARS — developer builds only
const char            *last_write_location{ nullptr }; // __FILE__:__LINE__ string
```

`write_count` being atomic means it can be read safely from any thread for profiling without adding a lock to the write path.

### Context-level statistics (`CmdCvarStats`)

```cpp
struct CmdCvarStats {
    // Always-on (≤1 relaxed atomic per event)
    std::atomic<uint64_t> cvars_written{ 0 };           // total cvar value writes
    // XASH_STATS
    std::atomic<uint64_t> commands_executed{ 0 };       // total commands dispatched
    std::atomic<uint64_t> commands_dropped{ 0 };        // filtered by privilege check
    std::atomic<uint32_t> buffer_high_water{ 0 };       // peak deque depth (entries)
    uint32_t              peak_cvar_count{ 0 };         // max cvars registered at once
    uint32_t              peak_command_count{ 0 };      // max commands registered at once
};
```

Exposed via `CmdCvarContext::stats()` returning a `const CmdCvarStats &`. Consumers snapshot the values; no locking needed because individual fields are atomic or written game-thread-only.

### Per-command execution count

```cpp
// Inside CommandDesc, XASH_STATS only
std::atomic<uint64_t> execute_count{ 0 };
```

Lets a profiler answer "which commands are invoked most often per frame" cheaply.

### Cvar change log (`XASH_DEBUG_CVARS` only)

A fixed-size circular buffer on `CmdCvarContext` recording the last N cvar changes:

```cpp
struct CvarChangeRecord {
    const char     *cvar_name;      // stable pointer into registry
    char            old_value[64];  // truncated if longer
    char            new_value[64];
    uint32_t        frame;
    CvarWriteSource source;
};
// ring capacity defined in limits.hpp as cvar_change_log_capacity (default 256)
CircularBuffer<CvarChangeRecord, limits::cvar_change_log_capacity> change_log;
```

Exposed via `CmdCvarContext::change_log()`. Dump it on crash or via a `cvar_changelog` console command (registered only in debug builds).

### Break-on-write (`XASH_DEBUG_CVARS` only)

```cpp
// Set via debug API: ctx.debug_break_on_cvar_write("sv_cheats")
// Set to nullptr to clear.
const char *break_on_write_name{ nullptr };
```

Inside `Cvar::notify_changed()`:

```cpp
#if XASH_DEBUG_CVARS
    if (ctx_.break_on_write_name &&
        std::strcmp(name_, ctx_.break_on_write_name) == 0)
        XASH_DEBUG_BREAK(); // platform-specific: __debugbreak() / __builtin_trap()
#endif
```

Lets a developer attach a debugger and set a data watch without modifying any source. `cvar_breakonwrite <name>` console command (debug builds only) is the runtime interface.

### Hash bucket diagnostics (`XASH_DEBUG_CVARS` only)

```cpp
void CmdCvarContext::dump_hash_stats() const;
// Prints: bucket count, filled buckets, max chain length, average chain length.
// Used to tune limits::cvar_hash_buckets if mods with unusual cvar counts are observed.
```

## Threading

cmd_cvar is **main-thread engine state** (decision D8). It is not internally
synchronised; concurrent callers must serialise externally.

| Component | Constraint |
|-----------|-----------|
| `CmdCvarContext::init` / `shutdown` | Main thread only — assert `ThreadRole::Main` at entry (`::xash::core::assert_thread_role`) |
| `set_server_dll_loaded` / `set_client_dll_loaded` | Main thread only — assert `ThreadRole::Main` at entry (DLL load/unload is a main-thread event) |
| Cvar/command register, set, unlink, `cbuf_execute` / `cmd_execute_string` | Main (game) thread only; registry structure is frozen during runtime (D7) |
| Read-only query accessors (`cvar_variable_*`, `cmd_argc`/`cmd_argv`, `stats`) | Called on the owning (main) thread; no internal locking |
| Per-cvar `generation` (atomic), XASH_STATS `write_count` (atomic) | Safe to read from any thread — lock-free change detection (D8) |
| `CmdCvarStats` counters | `std::atomic` (or written game-thread-only); safe to snapshot from any thread |
| `tls_ctx` | Thread-local; set to the executing context for the duration of a single dispatch and nulled afterwards |

The four public mutators flagged by the TH-Role sweep (init, shutdown,
set_server_dll_loaded, set_client_dll_loaded) carry the `assert_thread_role`
guard. The remaining mutators (`cvar_set*`, `cmd_add`, `cbuf_*`, `cmd_execute_string`)
are not individually asserted: they run inside the same main-thread dispatch and
are exercised by the unit tests on the test thread, so a blanket per-entry assert
would false-trip the harness. See per-header `@thread-safety` contracts.

## Constant classification (Q-O)

Every tunable buffer/count constant lives in `limits.hpp` as an
`inline constexpr` and is referenced absolutely (`::xash::limits::*`) — see D6.
The residual numeric literals flagged by `limits_scan` are **not** tunables and
deliberately stay inline:

| Literal | Site | Classification |
|---|---|---|
| `15` | `compat_goldsrc.cpp` `std::array<std::string_view, 15> kFilterableExemptions` | **frozen** — self-sizing the constexpr GoldSrc HL-mod exemption table (each entry a reviewed exception, D12); the size is derived from the initializer, not a policy knob |
| `64` | `cvar.hpp` `CvarChangeRecord::old_value/new_value[64]` | **frozen (local)** — `XASH_DEBUG_CVARS` change-log truncation width; a developer-only fixed field, not shared across subsystems |
| `64` | `context_init.cpp` `char buf[64]` (`cmdlist`/`cvarlist` summary) | **local scratch** — stack buffer for a single `snprintf("%zu command(s)")` line |
| `128` | `context_misc.cpp` `char buf[128]` (`dump_hash_stats`) | **local scratch** — stack buffer for a single formatted hash-stats line |

None of these are cross-subsystem tunables, so promoting them to `limits.hpp`
would add dead knobs rather than remove magic numbers. (They are also out of
scope for the 6B S5 carve, which does not touch `limits.hpp`.)

## Q-11 Satellite Verdict

cmd_cvar has **no external satellite subsystem** (Q-11 inapplicable). The GoldSrc
compatibility layer (`compat_goldsrc.cpp` / `compat_null.cpp`) is **not** a
satellite: it is an in-tree implementation of the `ICompatPolicy` interface that
cmd_cvar itself owns, selected at link time by the `XASH_GOLDSRC_COMPAT` CMake
option (D12). This is the Q-12 compat-isolation seam — core files contain zero
`#ifdef HACKS_RELATED_HLMODS`, and every quirk is a reviewed `constexpr` table
entry — not a separately-scored satellite service. ✓
