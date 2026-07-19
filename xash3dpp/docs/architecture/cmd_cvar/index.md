# cmd_cvar — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `cmd_cvar/context.hpp` | `xash::cmd_cvar` | `CmdCvarContext`, `CmdCvarInitParams`, `CmdCvarStats` |
| `cmd_cvar/cvar.hpp` | `xash::cmd_cvar` | `CvarAbi`, `Cvar`, `CvarFlags`, `CvarType`, `CvarWriteSource`, `CvarDesc`, `CvarChangeRecord` |
| `cmd_cvar/command.hpp` | `xash::cmd_cvar` | `CommandFn`, `CommandFlags`, `CommandDesc` |
| `cmd_cvar/observers.hpp` | `xash::cmd_cvar` | `ICvarObserver`, `ITrustOracle` |

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/cmd_cvar/context_impl.hpp` | `CmdCvarContext::Impl` struct definition; `tls_ctx` thread-local; `cvar_list_next`, `pool_dup` helpers |
| `private/cmd_cvar/cmd_hash_map.hpp` | `CmdHashMap<V>`: case-insensitive fixed-bucket hash map for cvars, commands, and aliases |
| `private/cmd_cvar/circular_buffer.hpp` | `CircularBuffer<T,N>`: fixed-capacity ring buffer for the debug change log |
| `private/cmd_cvar/compat_policy.hpp` | `ICompatPolicy` interface (GoldSrc quirks); included only by the host layer and compat TUs |
| `private/cmd_cvar/registry_types.hpp` | `Command`, `AliasDef` internal record types |

## Source files

| File | Responsibility |
|------|----------------|
| `context.cpp` | Pimpl construction, destruction, and move operations |
| `context_init.cpp` | `CmdCvarContext::init()` and `shutdown()` |
| `context_misc.cpp` | `stats()`, `dump_hash_stats()`, `debug_break_on_cvar_write()` |
| `cvar.cpp` | Cvar skeleton (unused includes removed; ops in `cvar_ops.cpp`) |
| `cvar_ops.cpp` | `cvar_register_engine`, `cvar_register_dll`, `cvar_set`, `cvar_set_direct`, `cvar_unlink`, `cvar_describe`, `cvar_variable_*`, `cvar_find`, `cvar_get_or_create`, `cvar_full_set`, `cvar_set_cheat_state`, `cvar_write_variables`, `cvar_prepare_to_unlink`, `unlink_pending_cvars` |
| `cmd.cpp` | Command skeleton (unused includes removed; ops in `cmd_ops.cpp`) |
| `cmd_ops.cpp` | `cmd_add`, `cmd_remove`, `cmd_unlink`, `cmd_describe`, `cmd_exists`, `cmd_execute_string` |
| `cmd_dispatch.cpp` | `cbuf_execute`, tokenizer, privilege dispatch |
| `base_cmd.cpp` | Placeholder TU (static_asserts only) — reserved for the sorted autocomplete table (HB-11); built-ins actually register in `context_init.cpp` |
| `compat_goldsrc.cpp` | `GoldSrcCompatPolicy` implementation (linked when `XASH_GOLDSRC_COMPAT=1`) |
| `compat_null.cpp` | `NullCompatPolicy` implementation (linked when `XASH_GOLDSRC_COMPAT=0`) |

## Key types

| Type | Kind | Defined in | Role |
|------|------|------------|------|
| `CmdCvarContext` | class (pimpl) | `context.hpp` | Owns all command and cvar state; public API surface |
| `CmdCvarInitParams` | struct | `context.hpp` | Named-parameters struct for `init()` |
| `CmdCvarStats` | struct | `context.hpp` | Lifetime counters (conditional on `XASH_STATS`) |
| `CvarAbi` | struct | `cvar.hpp` | ABI-frozen layout identical to legacy `cvar_t` |
| `Cvar` | struct | `cvar.hpp` | Full engine-internal cvar record; begins with `CvarAbi` at offset 0 |
| `CvarFlags` | enum | `cvar.hpp` | `FCVAR_*` bitmask values; frozen for DLL compat |
| `CvarType` | enum class | `cvar.hpp` | UI/scripting type hint |
| `CvarWriteSource` | enum class | `cvar.hpp` | Who triggered the last write (stats builds) |
| `CvarDesc` | struct | `cvar.hpp` | Copyable cvar snapshot for scripting/UI |
| `CvarChangeRecord` | struct | `cvar.hpp` | One entry in the debug change log (`XASH_DEBUG_CVARS`) |
| `CommandFn` | alias | `command.hpp` | `void (*)()` — layout-identical to legacy `xcommand_t` |
| `CommandFlags` | enum | `command.hpp` | `FCMD_*` bitmask values |
| `CommandDesc` | struct | `command.hpp` | Copyable command descriptor |
| `ICvarObserver` | interface | `observers.hpp` | Receives synchronous notifications on cvar writes |
| `ITrustOracle` | interface | `observers.hpp` | Answers whether the stuffcmd queue is currently trusted |
| `ICompatPolicy` | interface | `private/compat_policy.hpp` | Routes GoldSrc compatibility quirks |
| `CmdHashMap<V>` | template class | `private/cmd_hash_map.hpp` | Case-insensitive pool-backed hash map |
| `CircularBuffer<T,N>` | template class | `private/circular_buffer.hpp` | Fixed-capacity ring for the change log |
| `CmdCvarContext::Impl` | struct | `private/context_impl.hpp` | Pimpl body; complete only in `src/cmd_cvar/` TUs |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_cmd_cvar` | STATIC | `xash3dpp_utilities`, `xash3dpp_memory` | `xash3dpp_platform` |
| `test_cmd_cvar` | executable | `xash3dpp_cmd_cvar` | — |

### CMake options

| Option | Default | Effect |
|--------|---------|--------|
| `XASH_GOLDSRC_COMPAT` | `ON` | Links `compat_goldsrc.cpp` (GoldSrc quirk table) instead of `compat_null.cpp`; defines `XASH_GOLDSRC_COMPAT=1` preprocessor symbol |
